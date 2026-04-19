#!/usr/bin/env python3
"""
NorthSec Badge 2026 — Conference-Only Artifact Validator

Scans a conference-only firmware binary and package to ensure no CTF-sensitive
material leaked into the build. Intended for CI and pre-release checks.

Checks:
  1. Binary does not contain known CTF flag patterns
  2. Binary does not contain the string "swapboot"
  3. Binary does not contain challenge-specific identifiers
  4. Package does not include badge-ctf.bin
  5. Manifest mode is "conference-only"

Usage:
  python tools/validate_conference_only.py release/conference-only/
  python tools/validate_conference_only.py --bin-only badge-conference.bin
"""

import argparse
import json
import os
import re
import sys

# Patterns that must NOT appear in conference-only firmware binary
FORBIDDEN_PATTERNS = [
    # Flag format
    rb"NSEC\{",
    # Swapboot command string (should be compiled out)
    rb"swapboot",
    # Challenge module identifiers (quantum challenge strings)
    rb"CrystalState",
    rb"GridState",
    rb"quantum",
    rb"QAOA",
    rb"VQE",
    rb"[Cc]rystal.*[Tt]un",
    rb"[Gg]rid.*[Oo]pt",
    # Challenge CLI subcommands
    rb"challenges::quantum",
    rb"HAS_CHALLENGES",
]

# Files that must NOT exist in a conference-only package
FORBIDDEN_FILES = [
    "badge-ctf.bin",
]


def scan_binary(filepath):
    """Scan a binary file for forbidden patterns. Returns list of findings."""
    findings = []
    with open(filepath, "rb") as f:
        data = f.read()

    for pattern in FORBIDDEN_PATTERNS:
        matches = list(re.finditer(pattern, data))
        if matches:
            for m in matches:
                offset = m.start()
                # Extract context (up to 40 bytes around match)
                ctx_start = max(0, offset - 10)
                ctx_end = min(len(data), offset + len(m.group()) + 10)
                context = data[ctx_start:ctx_end]
                # Show printable representation
                printable = "".join(chr(b) if 32 <= b < 127 else "." for b in context)
                findings.append(
                    {
                        "pattern": pattern.decode("utf-8", errors="replace"),
                        "offset": f"0x{offset:08x}",
                        "context": printable,
                    }
                )

    return findings


def validate_package(pkg_dir):
    """Validate a conference-only package directory. Returns (ok, messages)."""
    messages = []
    ok = True

    # Check forbidden files
    for fname in FORBIDDEN_FILES:
        fpath = os.path.join(pkg_dir, fname)
        if os.path.exists(fpath):
            messages.append(f"FAIL: Forbidden file present: {fname}")
            ok = False

    # Check manifest
    manifest_path = os.path.join(pkg_dir, "manifest.json")
    if os.path.isfile(manifest_path):
        with open(manifest_path, "r") as f:
            manifest = json.load(f)
        if manifest.get("mode") != "conference-only":
            messages.append(f"FAIL: manifest.json mode is '{manifest.get('mode')}', expected 'conference-only'")
            ok = False
        else:
            messages.append("OK: manifest.json mode is 'conference-only'")

        # Ensure no CTF file entries in manifest
        for fi in manifest.get("files", []):
            if "ctf" in fi.get("name", "").lower():
                messages.append(f"FAIL: manifest references CTF file: {fi['name']}")
                ok = False
    else:
        messages.append("WARN: No manifest.json found")

    # Scan the conference binary
    bin_path = os.path.join(pkg_dir, "badge-conference.bin")
    if os.path.isfile(bin_path):
        findings = scan_binary(bin_path)
        if findings:
            ok = False
            messages.append(f"FAIL: badge-conference.bin contains {len(findings)} forbidden pattern(s):")
            for f in findings:
                messages.append(f"  pattern='{f['pattern']}' at {f['offset']}  context: {f['context']}")
        else:
            messages.append("OK: badge-conference.bin clean (no forbidden patterns)")
    else:
        messages.append("WARN: badge-conference.bin not found in package")

    return ok, messages


def validate_binary_only(bin_path):
    """Validate a single binary file."""
    if not os.path.isfile(bin_path):
        print(f"ERROR: File not found: {bin_path}")
        return False

    findings = scan_binary(bin_path)
    if findings:
        print(f"FAIL: {bin_path} contains {len(findings)} forbidden pattern(s):")
        for f in findings:
            print(f"  pattern='{f['pattern']}' at {f['offset']}  context: {f['context']}")
        return False
    else:
        print(f"OK: {bin_path} is clean")
        return True


def main():
    parser = argparse.ArgumentParser(description="Validate conference-only release artifacts")
    parser.add_argument("path", help="Package directory or binary file to validate")
    parser.add_argument("--bin-only", action="store_true", help="Validate a single binary file only")

    args = parser.parse_args()

    if args.bin_only:
        ok = validate_binary_only(args.path)
    else:
        ok, messages = validate_package(args.path)
        for msg in messages:
            print(msg)

    if ok:
        print("\nVALIDATION PASSED")
        sys.exit(0)
    else:
        print("\nVALIDATION FAILED — conference-only package may contain CTF material")
        sys.exit(1)


if __name__ == "__main__":
    main()
