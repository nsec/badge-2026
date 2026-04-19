NorthSec Badge 2026 — Conference Only Release
============================================================
Version: dev
Mode:    conference-only

Prerequisites:
  - Python 3.7+
  - esptool: pip install esptool

Quick Flash:
  python flash.py --mode conference-only --port <PORT>

Dry Run (preview without flashing):
  python flash.py --mode conference-only --dry-run

Files:
  0x0  bootloader.bin  (14.8 KB)  sha256:1776e4dd896a69d0...
  0x8000  partitions.bin  (3.0 KB)  sha256:f5facf0bb83d6dc4...
  0x10000  badge-conference.bin  (414.1 KB)  sha256:96b7568275340f9f...

Notes:
  - This package contains ONLY the conference firmware.
  - The CTF firmware is NOT included.
  - The 'swapboot' CLI command is disabled.
  - Safe for pre-CTF public distribution.
