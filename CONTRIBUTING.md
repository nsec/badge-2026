# Contributing to NorthSec Badge 2026

Thank you for contributing to the NorthSec Badge 2026 firmware!

## Development Workflow

### 1. Set Up Development Environment

Follow the setup instructions in [README.md](README.md).

**Windows users:** Remember to set `PLATFORMIO_CORE_DIR=C:\pio` before starting.

### 2. Branch Naming

- Feature: `feature-description`
- Bugfix: `bugfix-description`
- Challenge: `challenge-description`

### 3. Making Changes

```bash
# Create feature branch
git checkout -b feature-new-challenge
git push -u origin feature-new-challenge

# Make changes and commit
git add .
git commit -m "Add new challenge module"
git push
```

### 4. Pull Request Process

1. **Open PR** against `main` branch
2. **GitHub Actions** will automatically build both firmwares
3. **Review** - At least one maintainer approval required
4. **Merge** - Squash and merge preferred for clean history

### 5. Creating Releases

After merging to main, maintainers create release tags:

```bash
git checkout main
git pull
git tag v1.0.0
git push origin v1.0.0
```

GitHub Actions automatically builds and publishes release artifacts.

## Adding New Modules

### Conference Module (Conference Firmware)

Create files under `lib/conference/`:

**my_feature.h:**
```cpp
#pragma once
#include <Stream.h>

namespace conference {
namespace my_feature {

void init();
void registerCommands();
void handleMyCommand(Stream& stream);

} // namespace my_feature
} // namespace conference
```

**my_feature.cpp:**
```cpp
#include "my_feature.h"
#include <Arduino.h>
#include <../core/cli/cli.h>

namespace conference {
namespace my_feature {

void init() {
    Serial.println("  - My feature module loaded");
    registerCommands();
}

void registerCommands() {
    core::cli::registerCommand("mycommand", "do something cool", 
        [](Stream& stream, const String& args) {
            handleMyCommand(stream);
        });
}

void handleMyCommand(Stream& stream) {
    stream.println("Hello from my feature!");
}

} // namespace my_feature
} // namespace conference
```

**Register in lib/conference/registry.cpp:**
```cpp
#include "my_feature.h"

void init() {
    Serial.println("Conference modules initialized");
    schedule::init();
    my_feature::init();  // Add this line
}
```

### Challenge Module (CTF Firmware)

Same process, but under `lib/challenges/` and register in `lib/challenges/registry.cpp`.

## Testing

### Local Testing

```powershell
# Build both firmwares
C:\pio\penv\Scripts\platformio.exe run

# Flash and test conference
C:\pio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1-conference -t upload

# Flash and test CTF
C:\pio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1-ctf -t upload
```

### Test Checklist

- [ ] Conference firmware builds without errors
- [ ] CTF firmware builds without errors
- [ ] New commands appear in `help` output
- [ ] Commands work as expected
- [ ] No memory leaks (monitor with `info` command)
- [ ] Serial output clean and readable
- [ ] Boot switching still works (`boot conference` / `boot ctf`)

## Commit Messages

Follow conventional commits:

- `feat: add crypto challenge module`
- `fix: correct schedule display formatting`
- `docs: update flashing instructions`
- `refactor: simplify CLI command registration`
- `test: add validation for boot switching`

## Module Guidelines

### Conference Modules

**Purpose:** Badge features for conference attendees
- Social/networking features
- Schedule/event info
- Badge interactions
- Conference-specific utilities

**Should NOT contain:** CTF challenges, flags, or competition content

### Challenge Modules

**Purpose:** CTF competition challenges
- Cryptography puzzles
- Hardware challenges
- Reverse engineering
- Code analysis

**Should NOT contain:** Conference schedule or non-competition features

## License

By contributing, you agree that your contributions will be licensed under the
Apache-2.0 License.
