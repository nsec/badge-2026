# Badge Flashing Guide

## Two Distribution Modes

The badge firmware supports two release packages:

| Mode | Use Case | Contents |
|------|----------|----------|
| **conference-only** | Pre-CTF badge distribution | Conference firmware only, no `swapboot`, no CTF code |
| **dual** | Full badge experience | Conference + CTF firmware, `swapboot` enabled |

## Requirements

- Python 3.7+ with esptool: `pip install esptool`
- USB-C data cable
- Badge firmware package from GitHub release

## Quick Flash (Recommended)

Each release package includes `flash.py`. Extract the package and run:

```bash
# Conference-only (pre-CTF distribution)
python flash.py --mode conference-only --port <PORT>

# Dual firmware (full badge)
python flash.py --mode dual --port <PORT>
```

### Options

| Flag | Description |
|------|-------------|
| `--mode` | `conference-only` or `dual` (required for flashing) |
| `--port` | Serial port, e.g. `COM4` or `/dev/ttyACM0` (auto-detect if omitted) |
| `--all` | Flash ALL detected ESP32-S3 badges in parallel |
| `--baud` | Baud rate (default: 460800) |
| `--dry-run` | Print the esptool command without executing |
| `--bin-dir` | Directory containing binaries (default: script directory) |
| `--list-ports` | Show all serial ports and exit |

### Examples

```bash
# Single badge (auto-detect port — works on Windows, macOS, Linux)
python flash.py --mode dual

# Single badge with explicit port
python flash.py --mode dual --port COM4              # Windows
python3 flash.py --mode conference-only --port /dev/ttyACM0  # Linux
python3 flash.py --mode dual --port /dev/cu.usbmodem*  # macOS

# Multi-flash: flash ALL connected badges at once
python flash.py --mode conference-only --all

# Preview without flashing
python flash.py --mode dual --all --dry-run

# Debug port detection
python flash.py --list-ports
```

### Erase (Wipe Badge Clean)

```bash
# Erase a single badge
python flash.py --erase

# Erase a specific port
python flash.py --erase --port COM4

# Erase ALL connected badges
python flash.py --erase --all
```

After erasing, reflash with the desired mode.

### Multi-Flash (Mass Production)

Connect multiple badges via USB hubs, then:

```bash
python flash.py --mode conference-only --all
```

All detected ESP32-S3 badges flash in parallel. A summary report shows success/failure per port.

## Manual Flash (esptool)

### Conference-Only

```bash
python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 badge-conference.bin \
  0x2A2000 spiffs-conference.bin
```

### Dual Firmware

```bash
python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 badge-conference.bin \
  0x150000 badge-ctf.bin \
  0x2A2000 spiffs-ctf.bin
```

## Memory Map

| Address    | Size   | Content              | Present In          |
|------------|--------|----------------------|---------------------|
| 0x0        | ~15KB  | bootloader.bin       | Both modes          |
| 0x8000     | 3KB    | partitions.bin       | Both modes          |
| 0xE000     | 8KB    | OTA data selector    | Auto                |
| 0x10000    | 1.25MB | badge-conference.bin | Both modes          |
| 0x150000   | 1.25MB | badge-ctf.bin        | Dual only           |
| 0x290000   | 64KB   | Core dump partition  | Auto                |
| 0x2A2000   | ~1.4MB | SPIFFS filesystem    | Both (mode-specific)|

## Verification Checklists

### Conference-Only

After flashing:
- [ ] Boot banner shows `Mode: conference-only`
- [ ] CLI `help` does NOT list `swapboot`
- [ ] LED heartbeat blinks
- [ ] CLI commands respond

### Dual Firmware

After flashing:
- [ ] Boot banner shows `Mode: conference (dual)`
- [ ] CLI `help` lists `swapboot`
- [ ] `swapboot` reboots to CTF firmware
- [ ] CTF boot banner shows `Mode: ctf`
- [ ] `swapboot` returns to conference firmware

## Troubleshooting

**"Failed to connect":**
- Hold BOOT button while connecting USB
- Try lower baud rate: `--baud 115200`
- Check USB cable supports data (not charge-only)

**Badge not working after flash:**
- Erase flash: `python -m esptool --chip esp32s3 --port <PORT> erase_flash`
- Reflash all files

**Wrong partition boots:**
- Use CLI `swapboot` command
- Or reflash with the correct package

## Support

For issues: https://github.com/nsec/badge-2026/issues
