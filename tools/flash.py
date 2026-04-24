#!/usr/bin/env python3
"""
NorthSec Badge 2026 — Flash Script

Supports two distribution modes:
  conference-only : Flashes only the conference firmware (pre-CTF distribution)
  dual            : Flashes both conference and CTF firmware

Supports multi-flash: connect multiple badges and flash them all in parallel.

Usage:
  python flash.py --mode dual                          # auto-detect one badge
  python flash.py --mode dual --port /dev/ttyACM0      # explicit single port
  python flash.py --mode conference-only --all          # flash ALL connected badges
  python flash.py --mode dual --all --dry-run           # preview multi-flash
  python flash.py --erase                               # erase one badge
  python flash.py --erase --all                         # erase ALL connected badges
  python flash.py --list-ports                          # show detected ESP32-S3 ports

Works on Windows, macOS, and Linux.
Requires: esptool  (pip install esptool)
"""

import argparse
import json
import os
import subprocess
import sys
import threading
import time

# pyserial ships with esptool — always available
import serial.tools.list_ports  # type: ignore[import-untyped]

CHIP = "esp32s3"
DEFAULT_BAUD = 460800

# Flash addresses
ADDR_BOOTLOADER = "0x0"
ADDR_PARTITIONS = "0x8000"
ADDR_CONFERENCE = "0x10000"
ADDR_CTF = "0x150000"
ADDR_SPIFFS = "0x2A2000"

# Expected file names per mode
REQUIRED_FILES = {
    "conference-only": [
        ("bootloader.bin", ADDR_BOOTLOADER),
        ("partitions.bin", ADDR_PARTITIONS),
        ("badge-conference.bin", ADDR_CONFERENCE),
        ("spiffs-conference.bin", ADDR_SPIFFS),
    ],
    "dual": [
        ("bootloader.bin", ADDR_BOOTLOADER),
        ("partitions.bin", ADDR_PARTITIONS),
        ("badge-conference.bin", ADDR_CONFERENCE),
        ("badge-ctf.bin", ADDR_CTF),
        ("spiffs-ctf.bin", ADDR_SPIFFS),
    ],
}

# USB VID/PID pairs for ESP32-S3 native USB (CDC-JTAG and CDC)
ESP32_S3_IDS = {
    (0x303A, 0x1001),  # ESP32-S3 USB JTAG/serial debug
    (0x303A, 0x0002),  # ESP32-S3 USB CDC
}

# Keywords that suggest an ESP32-S3 port (fallback heuristic)
ESP_KEYWORDS = ["ESP32", "ESP32-S3", "CP210", "CH340", "CH910", "UART", "USB Serial"]


# ---------------------------------------------------------------------------
# Port detection (OS-agnostic via pyserial)
# ---------------------------------------------------------------------------

def list_all_serial_ports():
    """Return all serial ports visible to the OS."""
    return list(serial.tools.list_ports.comports())


def is_esp32_port(port_info):
    """Heuristic: does this port look like an ESP32-S3 badge?"""
    # Match by known Espressif VID/PID
    if (port_info.vid, port_info.pid) in ESP32_S3_IDS:
        return True
    # Match by VID alone (Espressif vendor ID 0x303A)
    if port_info.vid == 0x303A:
        return True
    # Fallback: keyword match on description / manufacturer
    desc = (port_info.description or "").upper()
    mfr = (port_info.manufacturer or "").upper()
    for kw in ESP_KEYWORDS:
        if kw.upper() in desc or kw.upper() in mfr:
            return True
    return False


def detect_badge_ports():
    """Return list of serial port device strings that look like ESP32-S3 badges."""
    all_ports = list_all_serial_ports()
    badge_ports = [p.device for p in all_ports if is_esp32_port(p)]
    return sorted(set(badge_ports))


def detect_single_port():
    """Auto-detect a single badge port, preferring ESP32-S3 matches."""
    badges = detect_badge_ports()
    if badges:
        return badges[0]
    # Last resort: return first available port
    all_ports = list_all_serial_ports()
    if all_ports:
        return all_ports[0].device
    return None


def print_port_table():
    """Print a human-readable table of all serial ports."""
    all_ports = list_all_serial_ports()
    if not all_ports:
        print("No serial ports detected.")
        return

    print(f"{'Port':<20} {'VID:PID':<12} {'Description':<40} {'Badge?'}")
    print("-" * 80)
    for p in sorted(all_ports, key=lambda x: x.device):
        vid_pid = f"{p.vid:04X}:{p.pid:04X}" if p.vid is not None else "----:----"
        badge = "  *" if is_esp32_port(p) else ""
        desc = (p.description or p.manufacturer or "")[:40]
        print(f"{p.device:<20} {vid_pid:<12} {desc:<40} {badge}")


# ---------------------------------------------------------------------------
# File validation
# ---------------------------------------------------------------------------

def find_script_dir():
    return os.path.dirname(os.path.abspath(__file__))


def resolve_bin_dir(args):
    if args.bin_dir:
        return os.path.abspath(args.bin_dir)
    return find_script_dir()


def validate_files(bin_dir, mode):
    """Ensure all required binaries exist. Returns list of (path, addr) tuples."""
    missing = []
    entries = []
    for fname, addr in REQUIRED_FILES[mode]:
        full = os.path.join(bin_dir, fname)
        if not os.path.isfile(full):
            missing.append(fname)
        else:
            entries.append((full, addr))

    if missing:
        print(f"ERROR: Missing required files for '{mode}' mode in {bin_dir}:")
        for m in missing:
            print(f"  - {m}")
        sys.exit(1)

    return entries


# ---------------------------------------------------------------------------
# esptool invocation
# ---------------------------------------------------------------------------

def build_esptool_cmd(port, baud, entries):
    """Build the esptool write_flash command list."""
    cmd = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        CHIP,
        "--port",
        port,
        "--baud",
        str(baud),
        "--before",
        "default_reset",
        "--after",
        "hard_reset",
        "write_flash",
        "-z",
        "--flash_mode",
        "dio",
        "--flash_freq",
        "80m",
        "--flash_size",
        "8MB",
    ]
    for fpath, addr in entries:
        cmd.extend([addr, fpath])
    return cmd


def flash_one_badge(port, baud, entries, mode, label=""):
    """Flash a single badge. Returns (port, success, message)."""
    prefix = f"[{label or port}]"
    cmd = build_esptool_cmd(port, baud, entries)
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if result.returncode == 0:
            return (port, True, f"{prefix} SUCCESS")
        else:
            # Extract last few lines of stderr for diagnostics
            err_lines = (result.stderr or result.stdout or "").strip().splitlines()
            tail = "\n".join(err_lines[-5:]) if err_lines else "(no output)"
            return (port, False, f"{prefix} FAILED (exit {result.returncode})\n{tail}")
    except FileNotFoundError:
        return (port, False, f"{prefix} esptool not found — pip install esptool")
    except subprocess.TimeoutExpired:
        return (port, False, f"{prefix} TIMEOUT after 120s")


def erase_one_badge(port, baud, label=""):
    """Erase the entire flash of a single badge. Returns (port, success, message)."""
    prefix = f"[{label or port}]"
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", CHIP,
        "--port", port,
        "--baud", str(baud),
        "erase_flash",
    ]
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60,
        )
        if result.returncode == 0:
            return (port, True, f"{prefix} ERASED")
        else:
            err_lines = (result.stderr or result.stdout or "").strip().splitlines()
            tail = "\n".join(err_lines[-5:]) if err_lines else "(no output)"
            return (port, False, f"{prefix} ERASE FAILED (exit {result.returncode})\n{tail}")
    except FileNotFoundError:
        return (port, False, f"{prefix} esptool not found — pip install esptool")
    except subprocess.TimeoutExpired:
        return (port, False, f"{prefix} TIMEOUT after 60s")


# ---------------------------------------------------------------------------
# Multi-flash
# ---------------------------------------------------------------------------

def flash_multiple(ports, baud, entries, mode, dry_run=False):
    """Flash multiple badges in parallel. Returns (successes, failures) counts."""
    n = len(ports)
    print(f"\nFlashing {n} badge{'s' if n != 1 else ''} in parallel ({mode} mode)...")
    for i, port in enumerate(ports, 1):
        print(f"  [{i}/{n}] {port}")

    if dry_run:
        print("\n[DRY RUN] Would execute per port:")
        cmd = build_esptool_cmd("<PORT>", baud, entries)
        print(f"  {' '.join(cmd)}")
        return n, 0

    results = [None] * n
    threads = []

    def worker(idx, port):
        label = f"{idx + 1}/{n} {port}"
        results[idx] = flash_one_badge(port, baud, entries, mode, label)

    start = time.monotonic()
    for i, port in enumerate(ports):
        t = threading.Thread(target=worker, args=(i, port), daemon=True)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    elapsed = time.monotonic() - start

    # Report
    successes = 0
    failures = 0
    print(f"\n{'=' * 60}")
    print(f"Results ({elapsed:.1f}s):")
    print(f"{'=' * 60}")
    for port, ok, msg in results:
        status = "OK" if ok else "FAIL"
        icon = "+" if ok else "!"
        print(f"  [{icon}] {status}  {port}")
        if not ok:
            for line in msg.splitlines():
                print(f"         {line}")
            failures += 1
        else:
            successes += 1

    print(f"\nTotal: {successes} succeeded, {failures} failed out of {n}")
    return successes, failures


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="NorthSec Badge 2026 — Flash Script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples (single badge):
  python flash.py --mode conference-only --port COM4
  python flash.py --mode dual --port /dev/ttyACM0
  python flash.py --mode dual                            # auto-detect port

Examples (multi-flash):
  python flash.py --mode dual --all                      # flash ALL detected badges
  python flash.py --mode conference-only --all --dry-run  # preview without flashing

Erase:
  python flash.py --erase                                # erase one badge
  python flash.py --erase --all                          # erase ALL detected badges
  python flash.py --erase --port COM4                    # erase specific port

Utility:
  python flash.py --list-ports                           # show all serial ports
""",
    )
    parser.add_argument(
        "--mode",
        choices=["conference-only", "dual"],
        help="Flashing mode: 'conference-only' or 'dual' (conference + CTF)",
    )
    parser.add_argument("--port", help="Serial port (auto-detect if omitted)")
    parser.add_argument(
        "--all",
        action="store_true",
        help="Flash/erase ALL detected ESP32-S3 badges in parallel",
    )
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--bin-dir", help="Directory containing firmware binaries (default: script dir)")
    parser.add_argument("--dry-run", action="store_true", help="Print the esptool command without executing")
    parser.add_argument(
        "--erase",
        action="store_true",
        help="Erase entire flash (wipe badge clean). Combine with --all to erase multiple.",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="List all serial ports and exit",
    )

    args = parser.parse_args()

    # --list-ports: show ports and exit
    if args.list_ports:
        print("=== Serial Ports ===")
        print_port_table()
        badges = detect_badge_ports()
        if badges:
            print(f"\nDetected {len(badges)} ESP32-S3 badge(s): {', '.join(badges)}")
        else:
            print("\nNo ESP32-S3 badges detected.")
        return

    # --mode is required for flashing (but not for --erase or --list-ports)
    if not args.mode and not args.erase:
        parser.error("--mode is required (unless using --erase or --list-ports)")

    # ---- Erase mode ----
    if args.erase:
        if args.all:
            ports = detect_badge_ports()
            if not ports:
                print("ERROR: No ESP32-S3 badges detected for erase.")
                sys.exit(1)
            n = len(ports)
            print(f"=== Erasing {n} badge{'s' if n != 1 else ''} ===")
            for i, p in enumerate(ports, 1):
                print(f"  [{i}/{n}] {p}")
            if args.dry_run:
                print("\n[DRY RUN] Would erase all detected badges.")
                return
            results = [None] * n
            threads = []
            def erase_worker(idx, port):
                results[idx] = erase_one_badge(port, args.baud, f"{idx+1}/{n} {port}")
            for i, p in enumerate(ports):
                t = threading.Thread(target=erase_worker, args=(i, p), daemon=True)
                threads.append(t)
                t.start()
            for t in threads:
                t.join()
            failures = 0
            for port, ok, msg in results:
                icon = "+" if ok else "!"
                print(f"  [{icon}] {msg}")
                if not ok:
                    failures += 1
            print(f"\nErased: {n - failures} succeeded, {failures} failed")
            sys.exit(1 if failures else 0)
        else:
            port = args.port or detect_single_port()
            if not port:
                print("ERROR: No serial port found. Use --port or --list-ports.")
                sys.exit(1)
            print(f"=== Erasing badge on {port} ===")
            if args.dry_run:
                print(f"[DRY RUN] Would run: esptool --chip {CHIP} --port {port} erase_flash")
                return
            _, ok, msg = erase_one_badge(port, args.baud)
            print(msg)
            sys.exit(0 if ok else 1)

    bin_dir = resolve_bin_dir(args)
    mode = args.mode

    print("=== NorthSec Badge 2026 Flasher ===")
    print(f"Mode:      {mode}")
    print(f"Bin dir:   {bin_dir}")

    # Validate required files
    entries = validate_files(bin_dir, mode)

    print("Files to flash:")
    for fpath, addr in entries:
        size_kb = os.path.getsize(fpath) / 1024
        print(f"  {addr}  {os.path.basename(fpath)}  ({size_kb:.1f} KB)")

    # ---- Multi-flash mode ----
    if args.all:
        ports = detect_badge_ports()
        if not ports:
            print("\nERROR: No ESP32-S3 badges detected.")
            print("Connect badges via USB, then retry.  Use --list-ports to debug.")
            sys.exit(1)

        successes, failures = flash_multiple(ports, args.baud, entries, mode, args.dry_run)
        sys.exit(1 if failures else 0)

    # ---- Single-flash mode ----
    port = args.port
    if not port:
        port = detect_single_port()
        if not port:
            print("\nERROR: Could not auto-detect serial port.")
            print("Specify with --port, or use --list-ports to see available ports.")
            sys.exit(1)
        print(f"Port:      {port} (auto-detected)")
    else:
        print(f"Port:      {port}")

    cmd = build_esptool_cmd(port, args.baud, entries)

    if args.dry_run:
        print(f"\n[DRY RUN] Would execute:")
        print(f"  {' '.join(cmd)}")
        return

    print(f"\nFlashing badge ({mode})...")
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        print("\nERROR: esptool not found. Install with: pip install esptool")
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"\nERROR: Flashing failed (exit code {e.returncode})")
        sys.exit(1)

    print(f"\nSUCCESS: Badge flashed ({mode} mode)")
    if mode == "conference-only":
        print("  Badge boots to conference firmware. No CTF firmware present.")
    else:
        print("  Badge boots to conference firmware by default.")
        print("  Use 'swapboot' CLI command to switch to CTF firmware.")


if __name__ == "__main__":
    main()
