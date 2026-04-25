"""
NSec Badge Dock - Quantum Challenge Track (2026)
CircuitPython 9.2.x on ESP32-S3 DevKitC

Validates both Crystal Tuning (VQE) and Grid Optimization (QAOA) challenges.

Protocol:
  1. Dock detects badge at I2C address 0x68
  2. Dock requests CrystalState blob  [0x10, 0x03],  reads 42 bytes
  3. Dock requests GridState blob     [0x10, 0x04],  reads 24 bytes
  4. Dock independently recomputes expected energy from badge params
  5. Dock sends validation results + flags back to badge
  6. Dock sends LED color feedback

Validation model:
  - Badge stores canonicalized solve artifacts (params in milliradians, energy, CRC-32)
  - Dock recomputes energy from params using identical Hamiltonian (same PRNG seed)
  - Dock compares recomputed energy against badge-reported energy
  - Dock verifies energy is below solve threshold
  - Only then is the flag issued
"""

import board
import busio
import time
import math
import struct
import neopixel
import digitalio
import supervisor

# --- Pin Configuration (same as social dock) ---
PIN_NEOPIXEL = board.IO48
PIN_I2C_SCL = board.IO5
PIN_I2C_SDA = board.IO4
PIN_DETECT_OUT1 = board.IO9
PIN_DETECT_IN = board.IO10
PIN_DETECT_OUT2 = board.IO11

# --- Constants ---
BADGE_I2C_ADDR = 0x68

CMD_REQUEST_HWID = 0x01
CMD_SET_LED_COLOR = 0x02
CMD_CHALLENGE_DATA = 0x10

# Sub-opcodes (must match badge quantum.cpp)
CRYSTAL_REQUEST = 0x03
CRYSTAL_RESULT = 0x05
GRID_REQUEST = 0x04
GRID_RESULT = 0x06

LED_OFF = 0x00
LED_RED = 0x01
LED_GREEN = 0x02
LED_BLUE = 0x03

# --- Flags ---
CRYSTAL_FLAG = "NSEC{cryst4l_tun3d_VQE_2026}"
GRID_FLAG = "NSEC{gr1d_0pt1m1z3d_QAOA_2026}"

# --- Colors ---
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
PURPLE = (128, 0, 255)
CYAN = (0, 255, 255)
YELLOW = (255, 255, 0)
OFF = (0, 0, 0)
WHITE = (255, 255, 255)

# ===========================================================================
# Quantum simulation (pure Python, matches badge qsim.cpp exactly)
# ===========================================================================

def xorshift32(seed):
    """Same PRNG as badge coeff_rng / qsim xorshift32."""
    seed &= 0xFFFFFFFF
    seed ^= (seed << 13) & 0xFFFFFFFF
    seed ^= (seed >> 17)
    seed ^= (seed << 5) & 0xFFFFFFFF
    return seed & 0xFFFFFFFF

def rng_float(seed, lo, hi):
    seed = xorshift32(seed)
    u = seed / 4294967296.0
    return seed, lo + u * (hi - lo)

# ---------------------------------------------------------------------------
# Crystal Hamiltonian — TFIM chain (must match crystal.cpp)
# ---------------------------------------------------------------------------
NUM_CRYSTAL_QUBITS = 8
NUM_CRYSTAL_LAYERS = 2
NUM_CRYSTAL_PARAMS = 16

def build_crystal_hamiltonian():
    seed = 0x4E534543  # "NSEC"
    J = 1.0
    h = 1.5
    delta = []
    for _ in range(NUM_CRYSTAL_QUBITS):
        seed, v = rng_float(seed, -0.15, 0.15)
        delta.append(v)
    return J, h, delta

def crystal_evaluate(params_rad, J, h, delta):
    """Pure-Python TFIM VQE evaluation matching badge crystal.cpp."""
    nq = NUM_CRYSTAL_QUBITS
    dim = 1 << nq
    sv = [complex(0, 0)] * dim
    sv[0] = complex(1, 0)

    def gate_ry(sv, q, theta):
        co = math.cos(theta / 2)
        si = math.sin(theta / 2)
        mask = 1 << q
        for i in range(dim):
            if i & mask:
                continue
            i0, i1 = i, i | mask
            v0, v1 = sv[i0], sv[i1]
            sv[i0] = co * v0 - si * v1
            sv[i1] = si * v0 + co * v1

    def gate_cz(sv, q0, q1):
        m0, m1 = 1 << q0, 1 << q1
        for i in range(dim):
            if (i & m0) and (i & m1):
                sv[i] = -sv[i]

    # Ansatz: 2 layers of [RY per qubit, CZ chain]
    for layer in range(NUM_CRYSTAL_LAYERS):
        for q in range(nq):
            gate_ry(sv, q, params_rad[layer * nq + q])
        for q in range(nq - 1):  # chain, not ring
            gate_cz(sv, q, q + 1)

    def expect_z(q):
        val = 0.0
        mask = 1 << q
        for i in range(dim):
            p = abs(sv[i]) ** 2
            val += -p if (i & mask) else p
        return val

    def expect_zz(q0, q1):
        val = 0.0
        m0, m1 = 1 << q0, 1 << q1
        for i in range(dim):
            p = abs(sv[i]) ** 2
            par = ((1 if (i & m0) else 0) ^ (1 if (i & m1) else 0))
            val += -p if par else p
        return val

    def expect_x(q):
        val = 0.0
        mask = 1 << q
        for i in range(dim):
            if i & mask:
                continue
            i0, i1 = i, i | mask
            val += 2.0 * (sv[i0].real * sv[i1].real + sv[i0].imag * sv[i1].imag)
        return val

    energy = 0.0
    # -J Σ ZZ chain
    for i in range(nq - 1):
        energy -= J * expect_zz(i, i + 1)
    # -h Σ X
    for i in range(nq):
        energy -= h * expect_x(i)
    # Σ δᵢ Z
    for i in range(nq):
        energy += delta[i] * expect_z(i)
    return energy

def crystal_threshold(J, h, delta):
    nq = NUM_CRYSTAL_QUBITS
    bound = (nq - 1) * abs(J) + nq * abs(h) + sum(abs(d) for d in delta)
    return (-bound) * 0.642

# ---------------------------------------------------------------------------
# Grid Hamiltonian (must match grid.cpp with seed 0x47524944)
# ---------------------------------------------------------------------------
NUM_GRID_QUBITS = 10
NUM_GRID_SAMPLES = 256
GRID_SAMPLE_SEED = 0xDEADBEEF

def build_grid_hamiltonian():
    seed = 0x47524944  # "GRID"
    nq = NUM_GRID_QUBITS
    h = []
    for _ in range(nq):
        seed, v = rng_float(seed, -1.0, 1.0)
        h.append(v)

    edges = []
    J = {}
    # Ring
    for i in range(nq):
        j = (i + 1) % nq
        seed, w = rng_float(seed, -1.0, 1.0)
        J[(i, j)] = w
        edges.append((i, j, w))

    # 5 random extra edges
    for _ in range(5):
        seed = xorshift32(seed)
        i = seed % nq
        seed = xorshift32(seed)
        j = seed % nq
        if i == j:
            j = (j + 1) % nq
        if i > j:
            i, j = j, i
        if (i, j) not in J:
            seed, w = rng_float(seed, -0.5, 0.5)
            J[(i, j)] = w
            edges.append((i, j, w))

    # Precompute diagonal costs
    dim = 1 << nq
    diag = [0.0] * dim
    for z in range(dim):
        c = 0.0
        for qi in range(nq):
            si = -1.0 if (z & (1 << qi)) else 1.0
            c += h[qi] * si
        for (ei, ej, ew) in edges:
            si = -1.0 if (z & (1 << ei)) else 1.0
            sj = -1.0 if (z & (1 << ej)) else 1.0
            c += ew * si * sj
        diag[z] = c

    return h, edges, diag

def grid_evaluate(g1, g2, b1, b2, diag):
    """Pure-Python QAOA p=2 evaluation matching badge grid.cpp."""
    nq = NUM_GRID_QUBITS
    dim = 1 << nq
    amp = 1.0 / math.sqrt(dim)
    sv = [complex(amp, 0)] * dim

    def apply_cost(sv, gamma):
        for i in range(dim):
            angle = -gamma * diag[i]
            phase = complex(math.cos(angle), math.sin(angle))
            sv[i] *= phase

    def apply_rx_all(sv, beta):
        co = math.cos(beta / 2)
        si = math.sin(beta / 2)
        for q in range(nq):
            mask = 1 << q
            for i in range(dim):
                if i & mask:
                    continue
                i0, i1 = i, i | mask
                v0, v1 = sv[i0], sv[i1]
                sv[i0] = co * v0 + complex(0, -si) * v1
                sv[i1] = complex(0, -si) * v0 + co * v1

    # Layer 1
    apply_cost(sv, g1)
    apply_rx_all(sv, 2.0 * b1)
    # Layer 2
    apply_cost(sv, g2)
    apply_rx_all(sv, 2.0 * b2)

    # Deterministic sampling
    seed = GRID_SAMPLE_SEED
    energies = []
    for _ in range(NUM_GRID_SAMPLES):
        seed = xorshift32(seed)
        u = seed / 4294967296.0
        cumul = 0.0
        z = dim - 1
        for i in range(dim):
            cumul += abs(sv[i]) ** 2
            if u < cumul:
                z = i
                break
        energies.append(diag[z])

    energies.sort()
    best = energies[0]
    mean = sum(energies) / len(energies)
    tail = max(1, NUM_GRID_SAMPLES // 5)
    cvar = sum(energies[:tail]) / tail
    low_thresh = best + abs(best) * 0.1 + 0.5
    low_hits = sum(1 for e in energies if e < low_thresh)
    return best, mean, cvar, low_hits

def grid_optimal(diag):
    return min(diag)

def grid_threshold(diag):
    opt = grid_optimal(diag)
    return opt + abs(opt) * 0.05

def grid_cvar_threshold(diag):
    opt = grid_optimal(diag)
    return opt + abs(opt) * 0.20

GRID_MIN_HITS = 20

# ---------------------------------------------------------------------------
# Per-device parameter transform (must match grid.cpp transformParams)
# ---------------------------------------------------------------------------
import math

def compute_grid_transform(hwid_hex):
    #Compute per-device param transform from HWID hex string (e.g. '80B54EE0783C').#
    mac_bytes = bytes.fromhex(hwid_hex)
    seed = 0x51414F41  # "QAOA"
    for b in mac_bytes:
        seed = (seed * 31 + b) & 0xFFFFFFFF

    scales = []
    offsets = []
    for _ in range(4):
        seed = xorshift32(seed)
        scales.append(0.8 + (seed & 0xFFFF) / 65535.0 * 0.4)
        seed = xorshift32(seed)
        offsets.append(-0.3 + (seed & 0xFFFF) / 65535.0 * 0.6)
    return scales, offsets

def apply_grid_transform(g1, g2, b1, b2, hwid_hex):
    #Clamp params to [0, pi], then apply per-device affine transform.#
    PI = math.pi
    g1 = max(0.0, min(PI, g1))
    g2 = max(0.0, min(PI, g2))
    b1 = max(0.0, min(PI, b1))
    b2 = max(0.0, min(PI, b2))

    scales, offsets = compute_grid_transform(hwid_hex)
    g1 = g1 * scales[0] + offsets[0]
    g2 = g2 * scales[1] + offsets[1]
    b1 = b1 * scales[2] + offsets[2]
    b2 = b2 * scales[3] + offsets[3]
    return g1, g2, b1, b2

# ===========================================================================
# CRC-32 (matches badge crystal.cpp / grid.cpp)
# ===========================================================================

def crc32(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc ^ 0xFFFFFFFF

# ===========================================================================
# I2C helpers (same pattern as dock-quantum/code.py)
# ===========================================================================

pixel = neopixel.NeoPixel(PIN_NEOPIXEL, 1, brightness=0.3, auto_write=False)

detect_out1 = digitalio.DigitalInOut(PIN_DETECT_OUT1)
detect_out1.direction = digitalio.Direction.OUTPUT
detect_out1.value = True
detect_out2 = digitalio.DigitalInOut(PIN_DETECT_OUT2)
detect_out2.direction = digitalio.Direction.OUTPUT
detect_out2.value = True
detect_in = digitalio.DigitalInOut(PIN_DETECT_IN)
detect_in.direction = digitalio.Direction.INPUT
detect_in.pull = digitalio.Pull.DOWN

i2c = None
while not i2c:
    try:
        i2c = busio.I2C(scl=PIN_I2C_SCL, sda=PIN_I2C_SDA, frequency=400000)
    except Exception as e:
        print(f"I2C init retry: {e}")
        time.sleep(0.5)


def badge_present():
    while not i2c.try_lock():
        pass
    try:
        return BADGE_I2C_ADDR in i2c.scan()
    except OSError:
        return False
    finally:
        i2c.unlock()


def read_badge_hwid():
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_REQUEST_HWID]))
        time.sleep(0.05)
        buf = bytearray(12)
        i2c.readfrom_into(BADGE_I2C_ADDR, buf)
        return buf.decode("ascii", "replace").strip("\x00")
    except OSError as e:
        print(f"  HWID error: {e}")
        return None
    finally:
        i2c.unlock()


def request_blob(sub_opcode, size):
    """Send ChallengeData + sub_opcode, then read `size` bytes back."""
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_CHALLENGE_DATA, sub_opcode]))
        time.sleep(0.3)
        buf = bytearray(size)
        i2c.readfrom_into(BADGE_I2C_ADDR, buf)
        return buf
    except OSError as e:
        print(f"  Blob read error (op=0x{sub_opcode:02X}): {e}")
        return None
    finally:
        i2c.unlock()


def send_result(sub_opcode, success, flag_text=""):
    while not i2c.try_lock():
        pass
    try:
        result_byte = 0x01 if success else 0x00
        payload = bytes([CMD_CHALLENGE_DATA, sub_opcode, result_byte])
        if success and flag_text:
            payload += flag_text.encode("ascii")
        i2c.writeto(BADGE_I2C_ADDR, payload)
    except OSError as e:
        print(f"  Result send error: {e}")
    finally:
        i2c.unlock()


def set_badge_led(color):
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_SET_LED_COLOR, color]))
    except OSError:
        pass
    finally:
        i2c.unlock()

# ===========================================================================
# Validation logic
# ===========================================================================

# Pre-build Hamiltonians at startup
print("Building Crystal Hamiltonian...")
crystal_a, crystal_b, crystal_c = build_crystal_hamiltonian()
crystal_thresh = crystal_threshold(crystal_a, crystal_b, crystal_c)
print(f"  Crystal solve threshold: {crystal_thresh:.4f}")

print("Building Grid Hamiltonian...")
grid_h, grid_edges, grid_diag = build_grid_hamiltonian()
grid_thresh = grid_threshold(grid_diag)
print(f"  Grid solve threshold: {grid_thresh:.4f}")
print(f"  Grid CVaR threshold: {grid_cvar_threshold(grid_diag):.4f}")
print(f"  Grid min hits: {GRID_MIN_HITS}")
print(f"  Grid optimal: {grid_optimal(grid_diag):.4f}")


def validate_crystal(blob):
    """Validate a 42-byte CrystalState blob. Returns (success, details)."""
    if len(blob) < 42:
        return False, f"too short ({len(blob)} bytes)"
    if blob[0] == 0 and blob[1] == 0:
        return False, "empty state"

    # Unpack: version(u8) solved(u8) params(16×i16) energy(i32) checksum(u32)
    version = blob[0]
    solved = blob[1]
    params_raw = struct.unpack_from("<16h", blob, 2)  # 16 × int16 at offset 2
    energy_milli = struct.unpack_from("<i", blob, 34)[0]  # int32 at offset 34
    checksum = struct.unpack_from("<I", blob, 38)[0]  # uint32 at offset 38

    # Verify checksum (CRC-32 of first 38 bytes)
    expected_crc = crc32(blob[:38])
    if checksum != expected_crc:
        return False, f"CRC mismatch: got 0x{checksum:08X}, expected 0x{expected_crc:08X}"

    if version != 3:
        return False, f"version {version} != 3"

    # Convert milliradians to float radians
    params_rad = [mr / 1000.0 for mr in params_raw]
    badge_energy = energy_milli / 1000.0

    # Recompute energy independently
    dock_energy = crystal_evaluate(params_rad, crystal_a, crystal_b, crystal_c)

    # Compare: badge energy must match dock-recomputed within tolerance
    energy_diff = abs(dock_energy - badge_energy)
    tolerance = 0.05  # allow small float rounding difference
    if energy_diff > tolerance:
        return False, f"energy mismatch: badge={badge_energy:.4f} dock={dock_energy:.4f} diff={energy_diff:.4f}"

    # Check solve threshold
    if dock_energy >= crystal_thresh:
        return False, f"energy {dock_energy:.4f} >= threshold {crystal_thresh:.4f}"

    return True, f"energy={dock_energy:.4f} (threshold={crystal_thresh:.4f})"


def validate_grid(blob, hwid_hex):
    """Validate a 24-byte GridState blob. Returns (success, details)."""
    if len(blob) < 24:
        return False, "too short"
    if blob[0] == 0 and blob[1] == 0:
        return False, "empty state"

    # Unpack: version(u8) solved(u8) g1(i16) g2(i16) b1(i16) b2(i16)
    #         best_energy(i32) low_hits(u16) cvar(i32) checksum(u32)
    version = blob[0]
    solved = blob[1]
    g1_mr, g2_mr, b1_mr, b2_mr = struct.unpack_from("<4h", blob, 2)
    best_energy_milli = struct.unpack_from("<i", blob, 10)[0]
    low_hits = struct.unpack_from("<H", blob, 14)[0]
    cvar_milli = struct.unpack_from("<i", blob, 16)[0]
    checksum = struct.unpack_from("<I", blob, 20)[0]

    # Verify checksum (CRC-32 of first 20 bytes)
    expected_crc = crc32(blob[:20])
    if checksum != expected_crc:
        return False, f"CRC mismatch: got 0x{checksum:08X}, expected 0x{expected_crc:08X}"

    if version != 1:
        return False, f"version {version} != 1"

    # Convert milliradians to float
    g1 = g1_mr / 1000.0
    g2 = g2_mr / 1000.0
    b1 = b1_mr / 1000.0
    b2 = b2_mr / 1000.0
    badge_best = best_energy_milli / 1000.0
    badge_cvar = cvar_milli / 1000.0

    # Apply per-device parameter transform (must match badge grid.cpp)
    if hwid_hex:
        g1, g2, b1, b2 = apply_grid_transform(g1, g2, b1, b2, hwid_hex)

    # Recompute QAOA independently
    dock_best, dock_mean, dock_cvar, dock_hits = grid_evaluate(g1, g2, b1, b2, grid_diag)

    # Compare best energy
    energy_diff = abs(dock_best - badge_best)
    if energy_diff > 0.1:
        return False, f"best energy mismatch: badge={badge_best:.4f} dock={dock_best:.4f}"

    # Compare low hits
    hit_diff = abs(dock_hits - low_hits)
    if hit_diff > 5:
        return False, f"hit count mismatch: badge={low_hits} dock={dock_hits}"

    # Check solve criteria
    if dock_best >= grid_thresh:
        return False, f"best {dock_best:.4f} >= threshold {grid_thresh:.4f}"
    if dock_hits < GRID_MIN_HITS:
        return False, f"hits {dock_hits} < min {GRID_MIN_HITS}"
    cvar_thresh = grid_cvar_threshold(grid_diag)
    if dock_cvar >= cvar_thresh:
        return False, f"CVaR {dock_cvar:.4f} >= threshold {cvar_thresh:.4f}"

    return True, f"best={dock_best:.4f} hits={dock_hits} cvar={dock_cvar:.4f}"

# ===========================================================================
# Main loop
# ===========================================================================

print("\nNSec Quantum Dock v2 — Crystal + Grid")
print("=" * 40)

for _ in range(3):
    pixel.fill(PURPLE)
    pixel.show()
    time.sleep(0.2)
    pixel.fill(OFF)
    pixel.show()
    time.sleep(0.2)

print("Waiting for badge...")

while True:
  try:
    if not badge_present():
        pixel.fill((20, 0, 40))
        pixel.show()
        time.sleep(0.1)
        pixel.fill(OFF)
        pixel.show()
        time.sleep(0.1)
        continue

    print("\nBadge detected!")
    pixel.fill(WHITE)
    pixel.show()
    time.sleep(0.5)

    hwid = read_badge_hwid()
    if hwid:
        print(f"  HWID: {hwid}")

    # --- Crystal validation ---
    print("  [Crystal] Requesting state...")
    crystal_blob = request_blob(CRYSTAL_REQUEST, 42)
    crystal_ok = False
    if crystal_blob:
        crystal_ok, crystal_detail = validate_crystal(crystal_blob)
        if crystal_ok:
            print(f"  [Crystal] PASS: {crystal_detail}")
            send_result(CRYSTAL_RESULT, True, CRYSTAL_FLAG)
            set_badge_led(LED_GREEN)
            pixel.fill(GREEN)
            pixel.show()
            time.sleep(3)
        else:
            print(f"  [Crystal] FAIL: {crystal_detail}")
            set_badge_led(LED_RED)
            pixel.fill(YELLOW)
            pixel.show()
            time.sleep(2)
    else:
        print("  [Crystal] No data received")

    set_badge_led(LED_OFF)
    time.sleep(0.5)

    # --- Grid validation ---
    print("  [Grid] Requesting state...")
    grid_blob = request_blob(GRID_REQUEST, 24)
    grid_ok = False
    if grid_blob:
        grid_ok, grid_detail = validate_grid(grid_blob, hwid)
        if grid_ok:
            print(f"  [Grid] PASS: {grid_detail}")
            send_result(GRID_RESULT, True, GRID_FLAG)
            set_badge_led(LED_GREEN)
            pixel.fill(GREEN)
            pixel.show()
            time.sleep(3)
        else:
            print(f"  [Grid] FAIL: {grid_detail}")
            set_badge_led(LED_RED)
            pixel.fill(YELLOW)
            pixel.show()
            time.sleep(2)
    else:
        print("  [Grid] No data received")

    # Summary
    set_badge_led(LED_OFF)
    if crystal_ok and grid_ok:
        pixel.fill(GREEN)
        print("  === BOTH CHALLENGES SOLVED ===")
    elif crystal_ok or grid_ok:
        pixel.fill(CYAN)
        print(f"  Partial: Crystal={'PASS' if crystal_ok else 'FAIL'} Grid={'PASS' if grid_ok else 'FAIL'}")
    else:
        pixel.fill(RED)
        print("  Neither challenge solved")

    pixel.show()
    time.sleep(5)
    pixel.fill(OFF)
    pixel.show()

    print("Waiting for badge removal...")
    while badge_present():
        time.sleep(0.5)
    print("Badge removed.")
    time.sleep(1)

  except Exception as e:
    print(f"Error: {e}")
    print("Restarting in 10 seconds...")
    pixel.fill(RED)
    pixel.show()
    time.sleep(10)
    pixel.fill(OFF)
    pixel.show()
    supervisor.reload()
