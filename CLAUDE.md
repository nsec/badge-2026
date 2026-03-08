# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NorthSec 2026 CTF badge firmware targeting ESP32-S3. Arduino framework, PlatformIO build system, FreeRTOS RTOS. Dual-firmware architecture: `conference` and `ctf` images share `lib/core/` but link separate feature libraries.

## Build Commands

```bash
pio run                                              # Build all environments
pio run -e esp32-s3-devkitc-1-conference             # Build conference only
pio run -e esp32-s3-devkitc-1-ctf                    # Build CTF only
pio run -e native                                    # Build desktop simulator
pio run -t upload                                    # Flash to badge
pio device monitor                                   # Serial monitor (115200 baud)
./.pio/build/native/program                          # Run simulator
```

After flashing NFC changes, **power cycle the badge** (soft reset doesn't clear ST25R3916 MODE register).

Format C++ with: `./format-cpp` (runs clang-format per `.clang-format`). Requires `clang-format` installed (`brew install clang-format`).

**Always run `./format-cpp` before committing C++ changes.** CI will fail on unformatted code.

## Architecture

### RTOS Task Model

Five FreeRTOS tasks communicate via type-safe `Queue<T>` wrappers (no shared memory):

| Task | Priority | Role |
|------|----------|------|
| ButtonTask | 5 | Polls 6 buttons (15ms debounce), sends `ButtonPressEvent` |
| NfcTask | 3 | Reader (RFAL discovery) or NTAG213 emulator (deferred ISR) |
| LedTask | 3 | Processes `LedCommand` queue, drives 18x WS2812 on IO8 |
| ControllerTask | 2 | Central event router: buttons → NFC/LED/social commands |
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

### Dual Firmware / OTA

Partition layout: conference at `0x10000` (factory), ctf at `0x150000` (ota_0). CLI commands `boot conference` / `boot ctf` switch via `esp_ota_set_boot_partition()`.

### NVS Social Storage

Four social categories (0-255 progress) stored in NVS with XOR encryption (MAC-keyed) + CRC-8 integrity. See `lib/core/storage/nvs_social.*`.

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

New conference modules go in `lib/conference/`, CTF challenges in `lib/challenges/`. Register CLI commands via `core::cli::registerCommand()` and hook into the registry's `init()` function. See CONTRIBUTING.md for templates.

## Simulator

`pio run -e native` builds a desktop binary using FreeRTOS (submodule: `extern/FreeRTOS-Kernel`). Initialize with `git submodule update --init --recursive`. CLI works identically; hardware is stubbed.
