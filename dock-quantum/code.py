"""
NSec Badge Dock - Quantum Challenge (2026)
CircuitPython 9.2.x on ESP32-S3 DevKitC

Protocol:
  1. Dock detects badge via I2C scan (address 0x68)
  2. Dock sends ChallengeData + QUANTUM_REQUEST [0x10, 0x01]
  3. Dock reads 2 bytes from badge (hello1, hello2 NVS values)
  4. Dock validates the values
  5. Dock sends ChallengeData + QUANTUM_RESPONSE [0x10, 0x02, result, flag...]
  6. Dock sends SetLedColor [0x02, color] for visual feedback

Wiring: Same as social dock (IO4=SDA, IO5=SCL, IO48=NeoPixel)
"""

import board
import busio
import time
import neopixel
import digitalio
import supervisor

# --- Pin Configuration ---
PIN_NEOPIXEL = board.IO48
PIN_I2C_SCL = board.IO5
PIN_I2C_SDA = board.IO4
PIN_DETECT_OUT1 = board.IO9
PIN_DETECT_IN = board.IO10
PIN_DETECT_OUT2 = board.IO11

# --- Constants ---
BADGE_I2C_ADDR = 0x68

# Dock command opcodes (must match badge dock.h)
CMD_REQUEST_HWID = 0x01
CMD_SET_LED_COLOR = 0x02
CMD_CHALLENGE_DATA = 0x10

# Quantum sub-opcodes
QUANTUM_REQUEST = 0x01   # Dock → Badge: request quantum values
QUANTUM_RESPONSE = 0x02  # Dock → Badge: send validation result + flag

# LED colors
LED_OFF = 0x00
LED_RED = 0x01
LED_GREEN = 0x02
LED_BLUE = 0x03

# --- Quantum Challenge Configuration ---
# The "correct" value that earns the flag.
EXPECTED_QUANTUM1 = 42
FLAG = "FLAG{qu4ntum_3nt4ngl3d_2026}"

# --- Colors ---
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
PURPLE = (128, 0, 255)
OFF = (0, 0, 0)
WHITE = (255, 255, 255)

# --- Hardware Init ---
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

# --- Helper Functions ---

def badge_present():
    """Check if badge is on the I2C bus."""
    while not i2c.try_lock():
        pass
    try:
        return BADGE_I2C_ADDR in i2c.scan()
    except OSError:
        return False
    finally:
        i2c.unlock()


def read_badge_hwid():
    """Read the badge's 12-byte hardware ID."""
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


def request_quantum_values():
    """Send QUANTUM_REQUEST and read back 1 byte (quantum1)."""
    while not i2c.try_lock():
        pass
    try:
        # Send ChallengeData + QUANTUM_REQUEST
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_CHALLENGE_DATA, QUANTUM_REQUEST]))
        time.sleep(0.1)  # Give badge time to prepare response

        # Read the 1-byte response
        buf = bytearray(1)
        i2c.readfrom_into(BADGE_I2C_ADDR, buf)
        return buf[0]
    except OSError as e:
        print(f"  Quantum read error: {e}")
        return None
    finally:
        i2c.unlock()


def send_quantum_result(success, flag_text=""):
    """Send validation result + flag to badge."""
    while not i2c.try_lock():
        pass
    try:
        # Build payload: [CMD_CHALLENGE_DATA, QUANTUM_RESPONSE, result, flag...]
        result_byte = 0x01 if success else 0x00
        payload = bytes([CMD_CHALLENGE_DATA, QUANTUM_RESPONSE, result_byte]) + flag_text.encode("ascii")
        i2c.writeto(BADGE_I2C_ADDR, payload)
    except OSError as e:
        print(f"  Result send error: {e}")
    finally:
        i2c.unlock()


def set_badge_led(color):
    """Set badge LED color."""
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_SET_LED_COLOR, color]))
    except OSError:
        pass
    finally:
        i2c.unlock()


# --- Boot ---
print("NSec Quantum Dock 2026")
print("======================")
print(f"Expected: quantum1={EXPECTED_QUANTUM1}")

for _ in range(3):
    pixel.fill(PURPLE)
    pixel.show()
    time.sleep(0.2)
    pixel.fill(OFF)
    pixel.show()
    time.sleep(0.2)

# --- Main Loop ---
print("Waiting for badge...")

while True:
  try:
    if not badge_present():
        pixel.fill((20, 0, 40))  # dim purple breathing
        pixel.show()
        time.sleep(0.1)
        pixel.fill(OFF)
        pixel.show()
        time.sleep(0.1)
        continue

    print("Badge detected!")
    pixel.fill(WHITE)
    pixel.show()
    time.sleep(0.5)

    # Step 1: Read HWID
    hwid = read_badge_hwid()
    if hwid:
        print(f"  HWID: {hwid}")

    # Step 2: Request quantum values
    q1 = request_quantum_values()
    if q1 is None:
        print("  Failed to read quantum values")
        time.sleep(2)
        continue

    print(f"  Received: quantum1={q1}")

    # Step 3: Validate
    success = (q1 == EXPECTED_QUANTUM1)

    if success:
        print(f"  CORRECT! Sending flag: {FLAG}")
        send_quantum_result(True, FLAG)
        set_badge_led(LED_GREEN)
        pixel.fill(GREEN)
    else:
        print(f"  WRONG! Expected quantum1={EXPECTED_QUANTUM1}")
        send_quantum_result(False)
        set_badge_led(LED_RED)
        pixel.fill(RED)

    pixel.show()
    time.sleep(5)

    # Turn off
    set_badge_led(LED_OFF)
    pixel.fill(OFF)
    pixel.show()

    # Wait for removal
    print("Waiting for badge removal...")
    while badge_present():
        time.sleep(0.5)

    print("Badge removed. Waiting for next badge...")
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
