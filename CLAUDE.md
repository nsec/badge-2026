# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NorthSec 2026 CTF badge firmware targeting ESP32-S3. Arduino framework, PlatformIO build system, FreeRTOS RTOS. Dual-firmware architecture: `conference` and `ctf` images share `lib/core/` but link separate feature libraries.

## Build Commands

```bash
pio run                                              # Build default environments (conference + ctf)
pio run -e esp32-s3-devkitc-1-conference             # Build conference firmware (dual — swapboot enabled)
pio run -e esp32-s3-devkitc-1-conference-only        # Build conference-only firmware (swapboot disabled)
pio run -e esp32-s3-devkitc-1-ctf                    # Build CTF only
pio run -e native                                    # Build desktop simulator
pio run -t upload                                    # Flash to badge
pio device monitor                                   # Serial monitor (115200 baud)
./.pio/build/native/program                          # Run simulator
python tools/package_release.py --version vX.Y.Z     # Package release artifacts
python tools/validate_conference_only.py release/conference-only/  # Validate no CTF leaks
python tools/flash.py --mode dual --port <PORT>      # Flash badge (dual firmware)
python tools/flash.py --mode conference-only --port <PORT>  # Flash badge (conference only)
python tools/flash.py --mode dual --all               # Multi-flash all connected badges
python tools/flash.py --list-ports                     # Show detected serial ports
```

After flashing NFC changes, **power cycle the badge** (soft reset doesn't clear ST25R3916 MODE register).

Format C++ with: `./format-cpp` (runs clang-format per `.clang-format`). Requires `clang-format` installed (`brew install clang-format`).

**Always run `./format-cpp` before committing C++ changes.** CI will fail on unformatted code.

## Architecture

### RTOS Task Model

Six FreeRTOS tasks communicate via type-safe `Queue<T>` wrappers (no shared memory):

| Task | Priority | Role |
|------|----------|------|
| ButtonTask | 5 | Polls 6 buttons (15ms debounce), sends `ButtonPressEvent` |
| NfcTask | 3 | Reader, NTAG213 emulator (deferred ISR), or NFC-DEP P2P pairing |
| LedTask | 3 | Processes `LedCommand` queue, drives 18x WS2812 on IO8 |
| ControllerTask | 2 | Central event router: buttons → NFC/LED/display/social commands |
| DisplayTask | 1 | Processes `DisplayCommand` queue, renders e-ink content with timeouts |
| CliTask | 1 | Serial CLI with command registration, history, ANSI escape |

Plus a heartbeat software timer (500ms status LED toggle on IO7).

### Library Structure

- **`lib/core/`** — Shared by both firmwares: hardware drivers, RTOS tasks, CLI, OTA, NVS storage
- **`lib/conference/`** — Conference firmware modules (social, schedule). Built with `-D HAS_CONFERENCE`
- **`lib/challenges/`** — CTF firmware modules (crypto puzzles). Built with `-D HAS_CHALLENGES`
- **`lib/simulator/`** — Arduino/FreeRTOS stubs for native desktop builds

### NFC Subsystem (ST25R3916)

**Critical**: The RFAL library requires pre-build patches (`extra_scripts/patch_rfal.py`) for ESP32:
- **Deferred ISR**: ESP32 can't do SPI in ISR context. ISR sets `isr_pending` flag; `rfalWorker()` polls it + `digitalRead(int_pin)` fallback
- **Low-power disable**: Oscillator wake takes 700µs, missing REQA from phones
- **EOF suppression**: NFC-A 100% ASK causes false EOF that resets the listen state machine

Single `RfalRfST25R3916Class` instance (with real interrupt pin) serves both reader and emulator modes. Never use `int_pin = -1` — it completely breaks interrupt processing.

After modifying patches: `rm -rf .pio/libdeps` to force re-patching.

### E-Ink Display (SSD1681 via GxEPD2)

1.54" 200x200 BW display (GDEH0154D67) shares HSPI bus with NFC. Pins: CS=IO41, DC=IO42, RST=IO45, BUSY=IO46. Library: `zinggjm/GxEPD2`.

- **Detection**: `einkInit()` sends SSD1681 soft-reset via SPI and watches BUSY pin (INPUT_PULLDOWN). Missing display = all display ops become no-ops.
- **Rotation**: Always use `setRotation(1)` for correct orientation (FPC connector on right).
- **Bitmap format**: GxEPD2 inverted format (0xFF=white, 0x00=black, MSB first). Draw with `drawInvertedBitmap()`.
- **Bitmap conversion**: `./tools/bmp2header.sh <image> <output.h> <array_name>` converts BMP/PNG to PROGMEM C header. Requires ImageMagick (`magick`).
- **DisplayTask**: Receives `DisplayCommand` via queue. Shows mode screens (no timeout while active), scan results (15s), pair results (5s), social progress (10s), then reverts to nsec logo.
- **Layout**: Info screens use 200x105 half-logo on top + 95px text area below. Boot/idle shows full 200x200 logo.
- **SPI sharing**: Uses same `SPIClass(HSPI)` as NFC via `nfcSPI()`. ESP32 SPI transactions handle bus locking.

See `lib/core/hardware/eink.*`, `lib/core/tasks/display.*`, `lib/core/tasks/display_task.*`.

### Dual Firmware / OTA

Partition layout: conference at `0x10000` (factory), ctf at `0x150000` (ota_0). CLI command `swapboot` toggles between firmwares via `esp_ota_set_boot_partition()`.

**Conference-only mode**: Build with `-D CONFERENCE_ONLY=1` (env `esp32-s3-devkitc-1-conference-only`). Compiles out `swapboot`, prevents accidental inclusion of CTF code via `#error` guard in `badge_config.h`. Used for pre-CTF badge distribution.

### NVS Social Storage

Four social categories (0-255 progress) stored in NVS with XOR encryption (MAC-keyed) + CRC-8/CCITT integrity:
- **Social** — Citizens/Players (Down button)
- **Sponsor** — Vendors (Up button)
- **Light** — Light collection (Left button)
- **Attraction** — Attractions (Right button)

See `lib/core/storage/nvs_social.*`.

## Code Style

- C++17, LLVM-based clang-format, 120 column limit, 2-space indent
- Namespaces not indented, `SortIncludes: Never` (intentional grouping)
- Pointers: `int *p`, references: `const Foo &ref`
- Conventional commits: `feat:`, `fix:`, `docs:`, `refactor:`
- Branch naming: `feature-*`, `bugfix-*`, `challenge-*`

## Key Configuration

- `include/badge_config.h` — RTOS parameters (queue depths, task priorities, debounce times)
- `lib/core/hardware/board_pins.h` — All GPIO pin assignments
- `partitions/badge_factory_ota.csv` — Flash partition table

## Adding Modules

New conference modules go in `lib/conference/`, CTF challenges in `lib/challenges/` (each in its own subfolder, e.g. `lib/challenges/crypto/`). Register CLI commands via `core::cli::registerCommand()` and hook into the registry's `init()` function. See CONTRIBUTING.md for templates.

## CLI Commands

Built-in commands (registered in `lib/core/tasks/cli.cpp`):

| Command | Description |
|---------|-------------|
| `help` / `?` | Show command help |
| `info` | Print boot/partition info |
| `hwid` | Print hardware ID (MAC-based) |
| `ledtest [N]` | RGB LED test suite |
| `einktest` | E-ink display test (skipped if no display) |
| `buttontest` | Interactive button test |
| `nvstest <key> <val>` | Set social NVS value (social/sponsor/light/attraction/all) |
| `status` | Show social NVS values |
| `clear` | Clear screen |
| `reboot` | Reboot |
| `swapboot` | Toggle between conference/CTF firmware |

Conference and CTF modules register additional commands via `core::cli::registerCommand()`.

## Simulator

`pio run -e native` builds a desktop binary using FreeRTOS (submodule: `extern/FreeRTOS-Kernel`). Initialize with `git submodule update --init --recursive`. CLI works identically; hardware is stubbed.
