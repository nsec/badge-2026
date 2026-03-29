"""
boot.py — runs before code.py on CircuitPython startup.
Disables USB mass storage so CircuitPython can write seen_badges.txt.

To regain USB write access for updating files:
  1. Connect serial, press Ctrl+C for REPL
  2. import os; os.remove("/boot.py")
  3. Ctrl+D to reset — CIRCUITPY drive reappears
  4. Copy files, then restore boot.py
"""
import storage
storage.disable_usb_drive()
