NorthSec Badge 2026 — Dual Release
============================================================
Version: dev
Mode:    dual

Prerequisites:
  - Python 3.7+
  - esptool: pip install esptool

Quick Flash:
  python flash.py --mode dual --port <PORT>

Dry Run (preview without flashing):
  python flash.py --mode dual --dry-run

Files:
  0x0  bootloader.bin  (14.8 KB)  sha256:1776e4dd896a69d0...
  0x8000  partitions.bin  (3.0 KB)  sha256:f5facf0bb83d6dc4...
  0x10000  badge-conference.bin  (414.9 KB)  sha256:5310238229c6eee6...
  0x150000  badge-ctf.bin  (440.8 KB)  sha256:102e15e5429fcb86...

Notes:
  - This package contains BOTH conference and CTF firmware.
  - Badge boots to conference firmware by default.
  - Use 'swapboot' CLI command to switch between firmwares.
