"""
Simulate a CTF player's journey solving Crystal.
Models what a player sees and how long each step takes on the badge.
"""
import math

def xor32(seed):
    seed = (seed ^ (seed << 13)) & 0xFFFFFFFF
    seed = (seed ^ (seed >> 17)) & 0xFFFFFFFF
    seed = (seed ^ (seed << 5)) & 0xFFFFFFFF
    return seed

NQ, NP, DIM = 8, 16, 256
J, H_FIELD = 1.0, 1.5

def build_delta():
    seed = 0x4E534543
    d = []
    for _ in range(NQ):
        seed = xor32(seed)
        d.append(-0.15 + (seed / 4294967296.0) * 0.3)
    return d

delta = build_delta()

def init_zero():
    sv = [0.0] * (2 * DIM)
    sv[0] = 1.0
    return sv

def gate_ry(sv, q, angle):
    c, s = math.cos(angle*0.5), math.sin(angle*0.5)
    step = 1 << q
    for i in range(DIM):
        if i & step: continue
        j = i | step
        i2, j2 = i*2, j*2
        ar, ai, br, bi = sv[i2], sv[i2+1], sv[j2], sv[j2+1]
        sv[i2], sv[i2+1] = c*ar - s*br, c*ai - s*bi
        sv[j2], sv[j2+1] = s*ar + c*br, s*ai + c*bi

def gate_cz(sv, q0, q1):
    m0, m1 = 1 << q0, 1 << q1
    for i in range(DIM):
        if (i & m0) and (i & m1):
            sv[i*2], sv[i*2+1] = -sv[i*2], -sv[i*2+1]

def expect_z(sv, q):
    v, m = 0.0, 1 << q
    for i in range(DIM):
        p = sv[i*2]**2 + sv[i*2+1]**2
        v += -p if (i & m) else p
    return v

def expect_zz(sv, q0, q1):
    v, m0, m1 = 0.0, 1 << q0, 1 << q1
    for i in range(DIM):
        p = sv[i*2]**2 + sv[i*2+1]**2
        v += ((-1 if (i&m0) else 1) * (-1 if (i&m1) else 1)) * p
    return v

def expect_x(sv, q):
    v, step = 0.0, 1 << q
    for i in range(DIM):
        if i & step: continue
        j = i | step
        v += 2*(sv[i*2]*sv[j*2] + sv[i*2+1]*sv[j*2+1])
    return v

def evaluate(params):
    sv = init_zero()
    for layer in range(2):
        for q in range(NQ):
            gate_ry(sv, q, params[layer*NQ + q])
        for q in range(NQ - 1):
            gate_cz(sv, q, q+1)
    e = 0.0
    for i in range(NQ-1): e -= J * expect_zz(sv, i, i+1)
    for i in range(NQ):   e -= H_FIELD * expect_x(sv, i)
    for i in range(NQ):   e += delta[i] * expect_z(sv, i)
    return e

bound = -((NQ-1)*J + NQ*H_FIELD + sum(abs(d) for d in delta))
threshold = bound * 0.64

def sweep(params, idx, lo, hi, steps):
    """Simulate badge sweep command. Returns (best_theta, best_energy)."""
    best_e, best_t = 999.0, params[idx]
    for s in range(steps):
        t = lo + (hi - lo) * s / (steps - 1)
        params[idx] = t
        e = evaluate(params)
        if e < best_e:
            best_e, best_t = e, t
    params[idx] = best_t
    return best_t, best_e

# ============================================================
# PLAYER JOURNEY SIMULATION
# ============================================================
print("=" * 60)
print("CTF PLAYER JOURNEY: Crystal Challenge")
print("=" * 60)
print()

# Phase 0: Discovery
print("PHASE 0: DISCOVERY")
print("-" * 40)
params = [0.0] * NP
e0 = evaluate(params)
print(f"> quantum crystal info")
print(f"  Threshold: {threshold:.4f}")
print(f"  Bound: {bound:.4f}")
print(f"  {NQ} qubits, {NP} params")
print()
print(f"> quantum crystal run")
print(f"  Energy: {e0:.4f} (all zeros)")
print(f"  Gap to threshold: {e0 - threshold:.4f}")
print()

# Phase 1: First exploration - single param sweep
print("PHASE 1: FIRST EXPLORATION (~5 min)")
print("-" * 40)
print("Player tries sweeping theta[0] to understand the tool:")
print(f"> quantum crystal sweep 0 -3.14 3.14 10")
t, e = sweep(params, 0, -3.14, 3.14, 10)
print(f"  Best theta[0]={t:.2f}, Energy={e:.4f}")
print(f"  Improvement: {e - e0:.4f}")
print()

# Phase 2: Sweep all params once
print("PHASE 2: SWEEP ALL 16 PARAMS (~20 min)")
print("-" * 40)
params = [0.0] * NP
badge_seconds = 0
for idx in range(NP):
    t, e = sweep(params, idx, -3.14, 3.14, 20)
    badge_seconds += 20 * 1.5  # ~1.5 sec per eval on badge
    if idx in [0, 7, 15]:
        print(f"  After θ[{idx:2d}]: E={e:.4f}")
e_r1 = evaluate(params)
print(f"  Round 1 complete: E={e_r1:.4f}  (threshold={threshold:.4f})")
print(f"  Gap: {e_r1 - threshold:.4f}  --> NOT SOLVED")
print(f"  Badge time: ~{badge_seconds/60:.0f} min")
print()

# Player sees "not solved" - this is the KEY MOMENT
print("PLAYER THOUGHT: 'It's close but not solved.'")
print("  'The hint says params interact. Let me sweep again.'")
print()

# Phase 3: Re-sweep rounds
print("PHASE 3: RE-SWEEP ROUNDS (~15 min each)")
print("-" * 40)
for rnd in range(2, 5):
    for idx in range(NP):
        t, e = sweep(params, idx, -3.14, 3.14, 20)
    e_rn = evaluate(params)
    solved = "SOLVED!" if e_rn < threshold else "not solved"
    print(f"  Round {rnd}: E={e_rn:.4f}  {solved}")

print()
print("PLAYER THOUGHT: 'Energy stalled at -12.54.'")
print("  'Hint says narrow sweep range to fine-tune.'")
print()

# Phase 4: Fine-tune
print("PHASE 4: FINE-TUNING (~15 min)")
print("-" * 40)
for frnd in range(1, 4):
    w = 0.15
    for idx in range(NP):
        center = params[idx]
        t, e = sweep(params, idx, center - w, center + w, 20)
    e_fn = evaluate(params)
    solved = "SOLVED!" if e_fn < threshold else ""
    print(f"  Fine round {frnd}: E={e_fn:.4f}  {solved}")
    if e_fn < threshold:
        break

print()
print(f"> quantum crystal store")
print(f"  Energy: {e_fn:.4f}  SOLVED")
print(f"  NVS store: success")
print()

# Summary
total_rounds = 3 + 1  # 3 coarse + 1 fine minimum
sweeps_per_round = NP
total_sweeps = total_rounds * sweeps_per_round
secs_per_sweep = 20 * 1.5  # 20 evals ~1.5s each on badge
total_badge_min = total_sweeps * secs_per_sweep / 60

print("=" * 60)
print("DIFFICULTY ANALYSIS")
print("=" * 60)
print()
print(f"Minimum path to solve:")
print(f"  3 coarse sweep rounds + 1 fine-tune = {total_sweeps} sweep commands")
print(f"  Badge time per sweep: ~30 sec (20 evals)")
print(f"  Total badge compute: ~{total_badge_min:.0f} min")
print(f"  Total wall clock (with typing/reading): ~{total_badge_min*1.5:.0f} min")
print()
print(f"Difficulty factors:")
print(f"  1. Must discover sweep → re-sweep pattern (hint in info)")
print(f"  2. Must realize coarse sweeps plateau at 63%")
print(f"  3. Must figure out narrow-range fine-tuning")
print(f"  4. 16 params × 4 rounds = {total_sweeps} commands to type")
print()
print(f"With LLM assistance:")
print(f"  - LLM can explain VQE/coordinate descent from info output")
print(f"  - LLM can generate the sweep commands in bulk")
print(f"  - Player still must execute {total_sweeps} commands on badge")
print(f"  - Badge is the bottleneck, not knowledge")
print()

# Alternative: what if someone writes set commands directly?
print(f"Alternative: direct parameter entry")
print(f"  If someone reverses the PRNG + writes their own solver,")
print(f"  they can compute optimal params offline, then:")
for i in range(NP):
    print(f"  quantum crystal set {i} {params[i]:.4f}")
print(f"  quantum crystal store")
print(f"  That's {NP+1} commands — fast but requires real quantum knowledge.")
