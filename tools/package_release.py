#!/usr/bin/env python3
"""
NorthSec Badge 2026 — Release Packaging Script

Gathers PlatformIO build outputs into release-ready packages for both
distribution modes:

  conference-only/
    bootloader.bin
    partitions.bin
    badge-conference.bin
    flash.py
    manifest.json
    README.txt

  dual-firmware/
    bootloader.bin
    partitions.bin
    badge-conference.bin
    badge-ctf.bin
    flash.py
    manifest.json
    README.txt

Usage:
  python tools/package_release.py [--version vX.Y.Z] [--output-dir release]
"""

import argparse
import hashlib
import json
import os
import shutil
import sys
from datetime import datetime, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# PlatformIO build output directories
BUILD_CONFERENCE = os.path.join(REPO_ROOT, ".pio", "build", "esp32-s3-devkitc-1-conference")
BUILD_CONFERENCE_ONLY = os.path.join(REPO_ROOT, ".pio", "build", "esp32-s3-devkitc-1-conference-only")
BUILD_CTF = os.path.join(REPO_ROOT, ".pio", "build", "esp32-s3-devkitc-1-ctf")

# Flash addresses
FLASH_MAP = {
    "bootloader.bin": "0x0",
    "partitions.bin": "0x8000",
    "badge-conference.bin": "0x10000",
    "badge-ctf.bin": "0x150000",
}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def copy_with_rename(src, dst):
    """Copy src to dst, creating parent directories as needed."""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)


def build_manifest(mode, version, files_info):
    """Create a manifest.json describing the release package."""
    return {
        "project": "NorthSec Badge 2026",
        "mode": mode,
        "version": version,
        "build_time": datetime.now(timezone.utc).isoformat(),
        "chip": "esp32s3",
        "flash_size": "8MB",
        "flash_mode": "dio",
        "flash_freq": "80m",
        "files": files_info,
    }


def generate_readme(mode, version, files_info):
    """Generate a human-readable README for the release package."""
    lines = [
        f"NorthSec Badge 2026 — {mode.replace('-', ' ').title()} Release",
        f"{'=' * 60}",
        f"Version: {version}",
        f"Mode:    {mode}",
        "",
        "Prerequisites:",
        "  - Python 3.7+",
        "  - esptool: pip install esptool",
        "",
        "Quick Flash:",
        f"  python flash.py --mode {mode} --port <PORT>",
        "",
        "Dry Run (preview without flashing):",
        f"  python flash.py --mode {mode} --dry-run",
        "",
        "Files:",
    ]
    for fi in files_info:
        lines.append(f"  {fi['address']}  {fi['name']}  ({fi['size_kb']:.1f} KB)  sha256:{fi['sha256'][:16]}...")

    if mode == "conference-only":
        lines.extend(
            [
                "",
                "Notes:",
                "  - This package contains ONLY the conference firmware.",
                "  - The CTF firmware is NOT included.",
                "  - The 'swapboot' CLI command is disabled.",
                "  - Safe for pre-CTF public distribution.",
            ]
        )
    else:
        lines.extend(
            [
                "",
                "Notes:",
                "  - This package contains BOTH conference and CTF firmware.",
                "  - Badge boots to conference firmware by default.",
                "  - Use 'swapboot' CLI command to switch between firmwares.",
            ]
        )

    lines.append("")
    return "\n".join(lines)


def package_mode(mode, output_dir, version, flash_script_path):
    """Package one release mode. Returns True on success."""
    pkg_dir = os.path.join(output_dir, mode)
    os.makedirs(pkg_dir, exist_ok=True)

    if mode == "conference-only":
        sources = {
            "bootloader.bin": os.path.join(BUILD_CONFERENCE_ONLY, "bootloader.bin"),
            "partitions.bin": os.path.join(BUILD_CONFERENCE_ONLY, "partitions.bin"),
            "badge-conference.bin": os.path.join(BUILD_CONFERENCE_ONLY, "firmware.bin"),
        }
    else:
        sources = {
            "bootloader.bin": os.path.join(BUILD_CONFERENCE, "bootloader.bin"),
            "partitions.bin": os.path.join(BUILD_CONFERENCE, "partitions.bin"),
            "badge-conference.bin": os.path.join(BUILD_CONFERENCE, "firmware.bin"),
            "badge-ctf.bin": os.path.join(BUILD_CTF, "firmware.bin"),
        }

    # Validate source files exist
    missing = []
    for name, src in sources.items():
        if not os.path.isfile(src):
            missing.append(f"{name} ({src})")
    if missing:
        print(f"ERROR: Missing build outputs for '{mode}' package:")
        for m in missing:
            print(f"  - {m}")
        return False

    # Copy binaries
    files_info = []
    for name, src in sources.items():
        dst = os.path.join(pkg_dir, name)
        copy_with_rename(src, dst)
        size_kb = os.path.getsize(dst) / 1024
        files_info.append(
            {
                "name": name,
                "address": FLASH_MAP[name],
                "size_bytes": os.path.getsize(dst),
                "size_kb": round(size_kb, 1),
                "sha256": sha256_file(dst),
            }
        )

    # Copy flash script
    shutil.copy2(flash_script_path, os.path.join(pkg_dir, "flash.py"))

    # Write manifest
    manifest = build_manifest(mode, version, files_info)
    with open(os.path.join(pkg_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)

    # Write README
    readme = generate_readme(mode, version, files_info)
    with open(os.path.join(pkg_dir, "README.txt"), "w") as f:
        f.write(readme)

    print(f"  Packaged '{mode}' -> {pkg_dir}")
    for fi in files_info:
        print(f"    {fi['address']}  {fi['name']}  ({fi['size_kb']:.1f} KB)")

    return True


def main():
    parser = argparse.ArgumentParser(description="NorthSec Badge 2026 — Release Packager")
    parser.add_argument("--version", default="dev", help="Version string (e.g. v1.0.0)")
    parser.add_argument("--output-dir", default=os.path.join(REPO_ROOT, "release"), help="Output directory")
    parser.add_argument(
        "--mode",
        choices=["conference-only", "dual", "all"],
        default="all",
        help="Which package(s) to create (default: all)",
    )

    args = parser.parse_args()
    flash_script = os.path.join(REPO_ROOT, "tools", "flash.py")

    if not os.path.isfile(flash_script):
        print(f"ERROR: flash.py not found at {flash_script}")
        sys.exit(1)

    print(f"=== NorthSec Badge 2026 Release Packager ===")
    print(f"Version: {args.version}")
    print(f"Output:  {args.output_dir}")

    ok = True
    if args.mode in ("conference-only", "all"):
        if not package_mode("conference-only", args.output_dir, args.version, flash_script):
            ok = False
    if args.mode in ("dual", "all"):
        if not package_mode("dual", args.output_dir, args.version, flash_script):
            ok = False

    if not ok:
        print("\nERROR: Some packages failed. Build all environments first:")
        print("  pio run -e esp32-s3-devkitc-1-conference-only")
        print("  pio run -e esp32-s3-devkitc-1-conference")
        print("  pio run -e esp32-s3-devkitc-1-ctf")
        sys.exit(1)

    print(f"\nDone. Release packages in: {args.output_dir}")


if __name__ == "__main__":
    main()
