# NorthSec Badge 2026 Firmware (ESP32-S3)

Bare-bones firmware scaffold for the **NorthSec 2026 CTF badge**, targeting **ESP32-S3**.

Design goals (repo requirements):

- **Arduino framework**, but **no `.ino`** (pure C++ entrypoint)
- **PlatformIO-based** build (produces flashable `.bin` outputs)
- **Two concurrent firmware images** via partitions: **`factory`** and **`ota_0`**
- A simple **serial CLI**, including a command to **switch which firmware boots next**
- Clear hierarchy separating:
  - `lib/core/` → hardware/system modules
  - `lib/challenges/` → CTF challenge modules
- GitHub Actions workflow builds on PR/push to `main`, and **publishes artifacts on version tags**

## Repo layout

```
src/
  main.cpp                     C++ Arduino entrypoint (no .ino)

lib/
  core/
    cli/                        Serial command line
    system/                     OTA boot selection / dual-firmware helpers
    hardware/                   Hardware abstraction (pins, drivers)
  challenges/
    registry.*                  Challenge init/tick hooks
    example_challenge.*         Placeholder challenge module

partitions/
  badge_factory_ota.csv         Partition table (factory + ota)

.github/workflows/
  build.yml                     CI build + tag-based release

platformio.ini                  PlatformIO project config
```

## PlatformIO + Arduino (no .ino)

This project uses PlatformIO with the Arduino framework.
The entrypoint is `src/main.cpp` (standard Arduino `setup()` / `loop()`), not a `.ino`.

## Dual firmware: `factory` + `ota_0`

The partition table `partitions/badge_factory_ota.csv` defines:

- `factory` (app) — a “golden” image
- `ota_0` (app) — an alternate/updated image
- `otadata` (data) — controls which app partition boots next

### How switching works

Under Arduino-ESP32, you can call ESP-IDF’s OTA APIs. This repo wraps them in:

- `lib/core/system/ota_manager.*`

That module provides:

- `core::ota::printBootInfo(Stream&)`
- `core::ota::setNextBoot(BootTarget, Stream&)`

## Serial CLI

The CLI is implemented in `lib/core/cli/` and runs over `Serial` at **115200**.

Commands:

- `help`
- `info` — print running partition and configured boot partition
- `boot factory` — set *next boot* to `factory` and reboot
- `boot ota` — set *next boot* to `ota_0` and reboot
- `reboot`

## Building locally

### Prerequisites

- **VS Code** with **PlatformIO IDE extension** installed, **OR**
- **PlatformIO Core** (CLI) installed

### Windows Setup (Important!)

**This project uses ESP-IDF framework to avoid Windows path-length issues with Arduino-ESP32 SDK extraction.**

Before building, set a short PlatformIO core directory to prevent path issues:

```powershell
# Set environment variable permanently (restart VS Code after)
setx PLATFORMIO_CORE_DIR C:\pio
```

Then **restart VS Code** or your terminal.

### First-Time Setup

1. Clone this repository:
   ```powershell
   git clone https://github.com/yourusername/badge-2026.git
   cd badge-2026
   ```

2. Set the short core directory (Windows only, see above)

3. Open the project in VS Code with PlatformIO extension, or use CLI

### Build

**In VS Code:** Click the PlatformIO "Build" button (checkmark icon) in the bottom toolbar

**Or via CLI:**
```powershell
# Build for factory partition (default)
C:\pio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1

# Or if platformio is in PATH
pio run
```

First build will download Arduino-ESP32 framework and toolchains. Subsequent builds are much faster.

### Dual Firmware Build

The badge uses a **factory + OTA** partition layout for dual-firmware boot:

- **factory** partition (`0x10000`) - Golden/stable image
- **ota_0** partition (`0x150000`) - Alternate/update image

**To flash both partitions:**

```powershell
# 1. Upload to factory partition
C:\pio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1 -t upload

# 2. Upload the same firmware to ota_0 partition
C:\pio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1-ota -t upload
```

**In VS Code:** Use the environment switcher in the bottom toolbar to select `esp32-s3-devkitc-1` or `esp32-s3-devkitc-1-ota`, then click Upload.

After flashing both, you can use the badge CLI commands to switch between them:
- `boot factory` - Set next boot to factory partition
- `boot ota` - Set next boot to ota_0 partition
- `info` - Show current boot partition

Build outputs (including flashable binaries) appear under:

- `.pio/build/<env>/firmware.bin`
- `.pio/build/<env>/bootloader.bin`
- `.pio/build/<env>/partitions.bin`

### Flash

If you have a board connected and the correct upload port set:

```powershell
pio run -t upload
```

### Monitor

```powershell
pio device monitor
```

## CI/CD - GitHub Actions Workflow

### Automated Builds

The workflow in `.github/workflows/build.yml` automatically builds firmware on:

- **Pull Requests** targeting `main` - Build verification
- **Push to `main`** - Build and upload artifacts
- **Version Tags** (`v*`) - Build, create GitHub Release with binaries

### What Gets Built

Both firmware variants are built in parallel:
- **Factory firmware** (`badge-factory.bin`) - Conference mode with schedule and social features
- **OTA firmware** (`badge-ota.bin`) - CTF challenges mode
- Supporting files: `bootloader.bin`, `partitions.bin`

### Creating a Release

**Step 1: Open a Pull Request**
```bash
git checkout -b feature-my-changes
# Make your changes
git add .
git commit -m "Add new feature"
git push origin feature-my-changes
```
Open PR on GitHub targeting `main` branch. GitHub Actions will build and verify.

**Step 2: Merge to Main**
After review, merge the PR. This triggers a build and uploads artifacts.

**Step 3: Create a Release Tag**
```bash
# Pull latest main
git checkout main
git pull origin main

# Create and push version tag
git tag v1.0.0
git push origin v1.0.0
```

Or use GitHub's web interface:
1. Go to repository → Releases → "Create a new release"
2. Click "Choose a tag" → Type new tag (e.g., `v1.0.0`) → "Create new tag"
3. Set release title (e.g., "NorthSec Badge 2026 v1.0.0")
4. GitHub Actions will automatically build and attach binaries

### Release Artifacts

Each release includes:
- `badge-factory.bin` - Conference firmware for factory partition
- `badge-ota.bin` - CTF challenges firmware for OTA partition
- `bootloader.bin` - ESP32-S3 bootloader
- `partitions.bin` - Partition table
- `FLASH_INSTRUCTIONS.txt` - Complete flashing guide

## Flashing Pre-Built Firmware

### Prerequisites

Install esptool:
```bash
pip install esptool
```

Find your serial port:
- **Windows**: Check Device Manager → Ports (COM & LPT) → Look for "USB Serial Device" (e.g., COM4)
- **Linux**: `ls /dev/ttyACM* /dev/ttyUSB*` (usually /dev/ttyACM0)
- **Mac**: `ls /dev/cu.*` (look for cu.usbmodem*)

### Download Release Files

Go to [Releases](https://github.com/yourusername/badge-2026/releases) and download the latest `.zip` or individual files.

### Flash All (Recommended for New Badges)

Flash bootloader, partition table, and both firmware images:

**Windows:**
```powershell
python -m esptool --chip esp32s3 --port COM4 --baud 460800 ^
  --before default_reset --after hard_reset write_flash -z ^
  --flash_mode dio --flash_freq 80m --flash_size 8MB ^
  0x0 bootloader.bin ^
  0x8000 partitions.bin ^
  0x10000 badge-factory.bin ^
  0x150000 badge-ota.bin
```

**Linux/Mac:**
```bash
python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 badge-factory.bin \
  0x150000 badge-ota.bin
```

### Update Only Firmware (Keep Existing Bootloader/Partitions)

If badges are already initialized, flash only updated firmware:

**Factory firmware only:**
```bash
python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
  write_flash -z 0x10000 badge-factory.bin
```

**OTA firmware only:**
```bash
python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
  write_flash -z 0x150000 badge-ota.bin
```

**Both firmwares:**
```bash
python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
  write_flash -z 0x10000 badge-factory.bin 0x150000 badge-ota.bin
```

### Verify Flash

Connect to serial console at 115200 baud:
```bash
# Windows
python -m serial.tools.miniterm COM4 115200

# Linux/Mac
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

You should see the boot banner and CLI prompt. Type `help` to see available commands.

### Switch Between Firmwares

At the CLI prompt:
- `boot factory` - Reboot to conference firmware
- `boot ota` - Reboot to CTF challenges firmware
- `info` - Show current running partition

## Additional Documentation

- **[FLASHING.md](FLASHING.md)** - Complete flashing guide for mass production and distribution
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Developer guide for adding features and challenges
- **[.github/workflows/build.yml](.github/workflows/build.yml)** - CI/CD configuration reference

## Firmware Architecture

### Dual-Firmware Design

This badge uses a unique dual-firmware architecture:

- **Factory Partition (0x10000)** - Conference mode
  - Boots by default on new badges
  - Contains conference-specific features (schedule, social, etc.)
  - Libraries: `core` + `conference`
  
- **OTA Partition (0x150000)** - CTF Challenges mode
  - Accessible via `boot ota` command
  - Contains competition challenges (crypto, hardware, etc.)
  - Libraries: `core` + `challenges`

Both firmwares share the `core` library (CLI, OTA management, hardware abstraction) but have completely different feature sets.

### Adding New Features

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed instructions on adding:
- Conference modules (factory firmware)
- Challenge modules (OTA firmware)
- Core utilities (shared by both)

## Notes / TODO

- Update `lib/core/hardware/board_pins.h` with the real badge pin mapping
- Add more conference features (badge pairing, social features)
- Add more CTF challenges (hardware, reverse engineering)
- Consider adding `ota_1` partition for triple-boot capability

## License

Apache-2.0 (see `LICENSE`).
# Northsec 2026 badge

## Hardware

## Firmware

## Credits
NorthSec CTF badge 2026 is brought to you by the teamwork of:
