"""
NSec Badge Dock - Simplified (2026)
CircuitPython 9.2.x on ESP32-S3 DevKitC

Behavior:
  1. Dock waits for a badge to be inserted (I2C slave appears at 0x68)
  2. Dock reads the badge's 12-char hex hardware ID via I2C
  3. Dock stores the ID in seen_badges.txt (only if unique)
  4. Both dock and badge LEDs flash blue for 5 seconds (suspense)
  5. 10% chance: green (lucky!) / 90% chance: red
  6. Dock LED shows the result color, badge LED is told the result via I2C
  7. After 3 seconds, everything resets and waits for next badge

Wiring (dock ESP32-S3 DevKitC):
  IO5  = I2C SCL (shared with badge via PCI connector)
  IO4  = I2C SDA (shared with badge via PCI connector)
  IO48 = WS2812 NeoPixel LED
  IO9  = Detect output 1 (drive HIGH)
  IO11 = Detect output 2 (drive HIGH)
  IO10 = Detect input (reads HIGH when badge is seated)
"""

import board
import busio
import time
import random
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
CMD_REQUEST_HWID = 0x01
CMD_SET_LED_COLOR = 0x02
CMD_SEND_DOCK_ID = 0x03

LED_OFF = 0x00
LED_RED = 0x01
LED_GREEN = 0x02
LED_BLUE = 0x03

WIN_PROBABILITY = 0.10  # 10% chance of green

SEEN_FILE = "seen_badges.txt"

# Read DOCK_ID from file, default to 1
try:
    with open("DOCK_ID", "r") as f:
        DOCK_ID = int(f.read().strip())
except Exception:
    DOCK_ID = 1
    print("Could not read DOCK_ID file, defaulting to", DOCK_ID)

# --- Colors ---
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
OFF = (0, 0, 0)
WHITE = (255, 255, 255)

# --- Hardware Init ---

# NeoPixel LED
pixel = neopixel.NeoPixel(PIN_NEOPIXEL, 1, brightness=0.3, auto_write=False)

# Detect pins — drive HIGH so badge can detect dock presence
detect_out1 = digitalio.DigitalInOut(PIN_DETECT_OUT1)
detect_out1.direction = digitalio.Direction.OUTPUT
detect_out1.value = True

detect_out2 = digitalio.DigitalInOut(PIN_DETECT_OUT2)
detect_out2.direction = digitalio.Direction.OUTPUT
detect_out2.value = True

detect_in = digitalio.DigitalInOut(PIN_DETECT_IN)
detect_in.direction = digitalio.Direction.INPUT
detect_in.pull = digitalio.Pull.DOWN

# I2C master
i2c = None
while not i2c:
    try:
        i2c = busio.I2C(scl=PIN_I2C_SCL, sda=PIN_I2C_SDA, frequency=400000)
    except Exception as e:
        print(f"I2C init retry: {e}")
        time.sleep(0.5)

# --- Helper Functions ---


def load_seen_badges():
    """Load the set of previously seen badge IDs from file."""
    seen = set()
    try:
        with open(SEEN_FILE, "r") as f:
            for line in f:
                hwid = line.strip()
                if hwid:
                    seen.add(hwid)
    except OSError:
        pass  # File doesn't exist yet
    return seen


def save_badge_id(hwid):
    """Append a badge ID to the seen file (only if unique)."""
    seen = load_seen_badges()
    if hwid in seen:
        print(f"  Badge {hwid} already seen ({len(seen)} total)")
        return False

    with open(SEEN_FILE, "a") as f:
        f.write(hwid + "\n")
    print(f"  NEW badge {hwid} saved! ({len(seen) + 1} total)")
    return True


def badge_present():
    """Check if a badge is inserted (I2C device responds at BADGE_I2C_ADDR)."""
    while not i2c.try_lock():
        pass
    try:
        return BADGE_I2C_ADDR in i2c.scan()
    except OSError:
        return False
    finally:
        i2c.unlock()


def read_badge_hwid():
    """Request and read the 12-byte hardware ID from the badge."""
    while not i2c.try_lock():
        pass
    try:
        # Send REQUEST_HWID command
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_REQUEST_HWID]))
        time.sleep(0.05)

        # Read 12 bytes (hex MAC string)
        buf = bytearray(12)
        i2c.readfrom_into(BADGE_I2C_ADDR, buf)

        hwid = buf.decode("ascii", "replace").strip("\x00")
        return hwid if len(hwid) == 12 else None
    except OSError as e:
        print(f"  HWID read error: {e}")
        return None
    finally:
        i2c.unlock()


def set_badge_led(color_code):
    """Tell the badge to set its LED color via I2C."""
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_SET_LED_COLOR, color_code]))
    except OSError:
        pass
    finally:
        i2c.unlock()


def send_dock_id():
    """Send this dock's ID to the badge so it can track sponsor progress."""
    while not i2c.try_lock():
        pass
    try:
        i2c.writeto(BADGE_I2C_ADDR, bytes([CMD_SEND_DOCK_ID, DOCK_ID]))
        print(f"  Sent dock ID: {DOCK_ID}")
    except OSError as e:
        print(f"  Failed to send dock ID: {e}")
    finally:
        i2c.unlock()


def flash_blue(duration_s=5.0):
    """Flash blue on both dock LED and badge LED for suspense."""
    set_badge_led(LED_BLUE)

    end_time = time.monotonic() + duration_s
    while time.monotonic() < end_time:
        # Flash on
        pixel.fill(BLUE)
        pixel.show()
        time.sleep(0.3)

        # Flash off
        pixel.fill(OFF)
        pixel.show()
        time.sleep(0.3)


def show_result(is_win):
    """Show green or red on dock LED and badge LED."""
    if is_win:
        color = GREEN
        badge_color = LED_GREEN
        print("  RESULT: GREEN (lucky!)")
    else:
        color = RED
        badge_color = LED_RED
        print("  RESULT: RED")

    # Set badge LED
    set_badge_led(badge_color)

    # Set dock LED
    pixel.fill(color)
    pixel.show()

    # Hold for 3 seconds
    time.sleep(3)

    # Turn everything off
    set_badge_led(LED_OFF)
    pixel.fill(OFF)
    pixel.show()


# --- Boot Animation ---

print("NSec Badge Dock 2026")
print(f"Dock ID: {DOCK_ID}")
print("====================")

for _ in range(3):
    pixel.fill(BLUE)
    pixel.show()
    time.sleep(0.2)
    pixel.fill(OFF)
    pixel.show()
    time.sleep(0.2)

seen = load_seen_badges()
print(f"Badges seen so far: {len(seen)}")

# --- Main Loop ---

print("Waiting for badge...")

while True:
  try:
    # Wait for badge to be inserted
    if not badge_present():
        # Quick pulse while idle
        pixel.fill((0, 0, 20))
        pixel.show()
        time.sleep(0.1)
        pixel.fill(OFF)
        pixel.show()
        time.sleep(0.1)
        continue

    print("Badge detected!")
    pixel.fill(WHITE)
    pixel.show()
    time.sleep(0.3)

    # Read hardware ID
    hwid = read_badge_hwid()
    if not hwid:
        print("  Failed to read HWID, retrying...")
        time.sleep(1)
        continue

    print(f"  HWID: {hwid}")

    # Send dock ID to badge (for sponsor tracking)
    send_dock_id()

    # Store if unique
    save_badge_id(hwid)

    # Suspense: flash blue for 5 seconds
    flash_blue(5.0)

    # Determine outcome: 10% green, 90% red
    is_win = random.random() < WIN_PROBABILITY
    show_result(is_win)

    # Wait for badge to be removed before scanning again
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
