#!/usr/bin/env python3
"""Test NFC P2P pairing between two badges."""

import serial
import threading
import time
import sys

PORT1 = "/dev/cu.usbmodem1101"
PORT2 = "/dev/cu.usbmodem31201"
BAUD = 115200
TIMEOUT = 30  # seconds to wait for pairing


def read_serial(ser, name, stop_event, lines):
    """Read from serial port and print with prefix."""
    while not stop_event.is_set():
        try:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                print(f"[{name}] {line}")
                lines.append(line)
        except serial.SerialException:
            break
        except Exception as e:
            if not stop_event.is_set():
                print(f"[{name}] Error: {e}")
            break


def wait_for_prompt(lines, timeout=5):
    """Wait until we see a '>' prompt in the output."""
    start = time.time()
    while time.time() - start < timeout:
        for line in lines:
            if ">" in line:
                return True
        time.sleep(0.1)
    return False


def send_pair(ser, name, lines):
    """Send pair command and verify it was received."""
    lines.clear()
    # Send a CR to get a fresh prompt
    ser.write(b"\r\n")
    time.sleep(0.3)
    ser.flushInput()
    lines.clear()
    # Send the pair command
    ser.write(b"pair\r\n")
    time.sleep(0.5)
    # Check if pair mode started
    for line in lines:
        if "Pair mode" in line or "PAIR" in line:
            return True
    # Try once more if not confirmed
    time.sleep(1.0)
    for line in lines:
        if "Pair mode" in line or "PAIR" in line:
            return True
    print(f"  WARNING: {name} may not have received pair command, retrying...")
    ser.write(b"pair\r\n")
    time.sleep(1.0)
    for line in lines:
        if "Pair mode" in line or "PAIR" in line:
            return True
    return False


def main():
    print(f"Opening {PORT1} and {PORT2}...")
    ser1 = serial.Serial(PORT1, BAUD, timeout=0.5)
    ser2 = serial.Serial(PORT2, BAUD, timeout=0.5)

    stop = threading.Event()
    lines1 = []
    lines2 = []

    t1 = threading.Thread(target=read_serial, args=(ser1, "BADGE1", stop, lines1), daemon=True)
    t2 = threading.Thread(target=read_serial, args=(ser2, "BADGE2", stop, lines2), daemon=True)
    t1.start()
    t2.start()

    # Wait for badges to boot and settle
    print("Waiting 4s for badges to be ready...")
    time.sleep(4)

    # Drain boot output
    lines1.clear()
    lines2.clear()

    # Send pair command to both badges
    print("Sending 'pair' to badge 1...")
    ok1 = send_pair(ser1, "BADGE1", lines1)
    print("Sending 'pair' to badge 2...")
    ok2 = send_pair(ser2, "BADGE2", lines2)

    if not ok1 or not ok2:
        print("WARNING: Not all badges confirmed pair mode start")

    # Merge all lines for detection
    lines1.clear()
    lines2.clear()

    # Wait for pairing result
    start = time.time()
    success1 = False
    success2 = False

    while time.time() - start < TIMEOUT:
        for line in lines1:
            if "VERIFIED" in line or "partner" in line:
                success1 = True
        for line in lines2:
            if "VERIFIED" in line or "partner" in line:
                success2 = True

        if success1 and success2:
            print("\n=== BOTH BADGES PAIRED SUCCESSFULLY ===")
            break

        if success1 or success2:
            which = "BADGE1" if success1 else "BADGE2"
            other = "BADGE2" if success1 else "BADGE1"
            # Give the other badge a bit more time
            if time.time() - start > 10:
                print(f"\n=== {which} paired but {other} did not respond in time ===")
                break

        time.sleep(0.5)
    else:
        print("\n=== TIMEOUT - pairing did not complete ===")

    stop.set()
    ser1.close()
    ser2.close()
    print("Done.")


if __name__ == "__main__":
    main()
