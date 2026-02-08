# NorthSec Badge 2026 - Complete Setup Summary

## 🎯 What You Have

A complete dual-firmware badge system with:
- **Conference firmware** - Conference features (schedule, social)
- **CTF firmware** - CTF challenges (crypto, puzzles)
- **CLI system** - Boot partition switching and modular commands
- **CI/CD pipeline** - Automated builds and releases
- **Complete documentation** - For developers, users, and production

## 📁 Repository Structure

```
badge-2026/
├── .github/workflows/
│   └── build.yml              # CI/CD: Build on PR, push, tags
├── lib/
│   ├── core/                  # Shared by both firmwares
│   │   ├── cli/               # Command-line interface
│   │   ├── hardware/          # Hardware abstraction
│   │   └── system/            # OTA boot management
│   ├── conference/            # Conference firmware only
│   │   ├── schedule.*         # Conference schedule module
│   │   └── registry.*         # Conference module loader
│   └── challenges/            # CTF firmware only
│       ├── crypto.*           # Crypto challenge module
│       └── registry.*         # Challenge module loader
├── partitions/
│   └── badge_factory_ota.csv  # Dual-partition layout
├── src/
│   └── main.cpp               # Arduino entry point
├── platformio.ini             # Build configuration
├── README.md                  # Main documentation
├── FLASHING.md                # Production flashing guide
├── CONTRIBUTING.md            # Developer guide
├── RELEASE.md                 # Release process for maintainers
└── LICENSE                    # Apache-2.0
```

## 🚀 Quick Start for Developers

### Setup
```powershell
# Windows: Set short path first
setx PLATFORMIO_CORE_DIR C:\pio

# Clone and build
git clone <repo-url>
cd badge-2026
C:\pio\penv\Scripts\platformio.exe run
```

### Daily Development
```powershell
# Build and flash both firmwares
C:\pio\penv\Scripts\platformio.exe run -t upload

# Or individually
C:\pio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1-conference -t upload        # Conference
C:\pio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1-ctf -t upload    # CTF
```

### Add New Feature
1. Create module files in `lib/core`, `lib/conference/` or `lib/challenges/`
2. Register in respective `registry.cpp`
3. Test locally
4. Create PR to `main`

## 🔄 Workflow: Code → Production

### 1. Development
```bash
git checkout -b feature-new-module
# ... make changes ...
git commit -m "feat: add new module"
git push origin feature-new-module
```

### 2. Pull Request
- Open PR on GitHub targeting `main`
- GitHub Actions builds both firmwares
- Review and merge

### 3. Create Release
```bash
git checkout main
git pull
git tag v1.0.0
git push origin v1.0.0
```

### 4. Automated Release
GitHub Actions automatically:
- Builds conference firmware → `badge-conference.bin`
- Builds CTF firmware → `badge-ctf.bin`
- Packages bootloader and partitions
- Creates GitHub Release with all files
- Attaches `FLASH_INSTRUCTIONS.txt`

### 5. Distribution
Download from GitHub Releases and flash:
```bash
python -m esptool --chip esp32s3 --port <PORT> write_flash -z \
  0x0 bootloader.bin 0x8000 partitions.bin \
  0x10000 badge-conference.bin 0x150000 badge-ctf.bin
```

## 📚 Documentation Index

| File | Audience | Purpose |
|------|----------|---------|
| [README.md](README.md) | Everyone | Setup, build, usage |
| [FLASHING.md](FLASHING.md) | Production/Distribution | Mass flashing guide |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Developers | Adding features |
| [RELEASE.md](RELEASE.md) | Maintainers | Release process |

## 🎮 Using the Badge

### Default Boot (Conference Mode)
```
> help
Commands:
  help       - show this help
  info       - partition info
  boot ctf   - switch to CTF mode
  schedule   - conference schedule
  ...

> schedule
=== NorthSec 2026 Schedule ===
...
```

### Switch to CTF (Challenges Mode)
```
> boot ctf
Rebooting to CTF challenges...

> help
Commands:
  help         - show this help
  info         - partition info
  boot conference - switch to conference mode
  crypto       - crypto challenge
  ...

> crypto
=== Crypto Challenge: The Cipher ===
...
```

## 🏭 Production Flashing

For mass production, see [FLASHING.md](FLASHING.md):
- Batch scripts included
- Quality assurance checklist
- Troubleshooting guide
- Memory map reference

## 🔧 Technical Details

### Memory Layout
```
0x000000   Bootloader (~15KB)
0x008000   Partition table (3KB)
0x00E000   OTA data selector (8KB)
0x010000   Conference firmware (1.25MB) - Conference
0x150000   CTF firmware (1.25MB) - Challenges
0x290000   Core dump (64KB)
0x2A0000   SPIFFS filesystem (1.4MB)
```

### Build System
- **PlatformIO** - Build framework
- **Arduino-ESP32** - Framework (via GitHub to avoid Windows issues)
- **Two environments**:
  - `esp32-s3-devkitc-1-conference` - Conference build with `HAS_CONFERENCE=1`
  - `esp32-s3-devkitc-1-ctf` - CTF build with `HAS_CHALLENGES=1`

### Module System
- Modular design - each feature in its own file
- Auto-registration via `registry.cpp`
- CLI commands registered at init
- Add/remove by including/excluding files

## ✅ Pre-Merge Checklist

Before merging to `main`:
- [ ] Both firmwares build successfully
- [ ] Tested on hardware
- [ ] CLI commands work
- [ ] Boot switching functional
- [ ] No compiler warnings
- [ ] Documentation updated

## 🎉 First Release Checklist

Ready for v1.0.0:
- [ ] All documentation complete
- [ ] Hardware tested thoroughly
- [ ] GitHub Actions workflow tested
- [ ] Flash instructions verified
- [ ] Quality assurance process defined
- [ ] Support channels established

## 🆘 Getting Help

- **Build issues?** Check [README.md](README.md) Windows setup section
- **Flashing issues?** See [FLASHING.md](FLASHING.md) troubleshooting
- **Contributing?** Read [CONTRIBUTING.md](CONTRIBUTING.md)
- **Questions?** Open GitHub Discussion
- **Bugs?** Open GitHub Issue

## 🎓 Next Steps

1. **Test the workflow**: Create a test PR and tag to verify CI/CD
2. **Add more modules**: Build out conference and challenge features
3. **Hardware customization**: Update `board_pins.h` with real pin mappings
4. **Production planning**: Review [FLASHING.md](FLASHING.md) for mass production
5. **Beta testing**: Flash test badges and gather feedback

## 🏆 Success Criteria

You'll know everything works when:
1. ✅ Push code → PR builds automatically
2. ✅ Merge → `main` builds successfully  
3. ✅ Tag `v1.0.0` → Release created with binaries
4. ✅ Download → Flash → Badge boots
5. ✅ Type `help` → See commands
6. ✅ Type `boot ctf` → Switches firmware
7. ✅ Type `boot conference` → Returns to conference mode

---

**You're ready to ship! 🚀**
