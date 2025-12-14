# Release Process Checklist

For maintainers creating firmware releases.

## Pre-Release Testing

- [ ] All PRs merged to `main`
- [ ] Local build succeeds: `pio run`
- [ ] Factory firmware tested on hardware
- [ ] OTA firmware tested on hardware
- [ ] Boot switching verified (`boot factory` ↔ `boot ota`)
- [ ] All CLI commands functional
- [ ] No compilation warnings
- [ ] Memory usage acceptable (check build output)

## Version Numbering

Use semantic versioning: `vMAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes, incompatible firmware updates
- **MINOR**: New features, challenges, backward compatible
- **PATCH**: Bug fixes, minor improvements

Examples:
- `v1.0.0` - Initial release
- `v1.1.0` - Added new challenge module
- `v1.1.1` - Fixed CLI bug

## Creating a Release

### Step 1: Update Version References

Update version strings in code if needed:
- `platformio.ini` - Check if version metadata should be updated
- `README.md` - Update any version-specific references

### Step 2: Create and Push Tag

```bash
# Ensure you're on latest main
git checkout main
git pull origin main

# Create annotated tag with release notes
git tag -a v1.0.0 -m "Release v1.0.0 - Initial NorthSec 2026 badge firmware

Features:
- Dual firmware (conference + CTF challenges)
- CLI with boot partition switching
- Conference schedule module
- Crypto challenge module
"

# Push tag to trigger release
git push origin v1.0.0
```

### Step 3: Monitor GitHub Actions

1. Go to repository → Actions
2. Watch the build workflow complete
3. Verify both environments build successfully
4. Check that artifacts are created

### Step 4: Verify Release

1. Go to repository → Releases
2. Verify the release was auto-created
3. Check all files are attached:
   - badge-factory.bin
   - badge-ota.bin
   - bootloader.bin
   - partitions.bin
   - FLASH_INSTRUCTIONS.txt
4. Review auto-generated release notes

### Step 5: Edit Release Notes (Optional)

Enhance the auto-generated release with:
- Highlights of new features
- Known issues
- Breaking changes
- Special flashing instructions

Example:
```markdown
## 🎉 NorthSec Badge 2026 - v1.0.0

First official release of the dual-firmware badge system!

### ✨ Features
- **Conference Mode**: Schedule, social features, badge interactions
- **CTF Mode**: Cryptography challenges and puzzles
- **Instant Switching**: Use CLI commands to switch between modes

### 📦 What's New
- Initial conference schedule module
- Crypto challenge: The Cipher
- CLI command registration system
- Automatic boot partition management

### 🐛 Bug Fixes
- Fixed line ending handling in CLI (CR/LF)
- Resolved core dump warnings at boot

### 📋 Flashing Instructions
See `FLASH_INSTRUCTIONS.txt` or [FLASHING.md](FLASHING.md) for complete guide.

Quick flash all:
\```bash
python -m esptool --chip esp32s3 --port <PORT> write_flash -z \
  0x0 bootloader.bin 0x8000 partitions.bin \
  0x10000 badge-factory.bin 0x150000 badge-ota.bin
\```

### 🔄 Upgrading from Previous Version
First release - no upgrade path needed.

### ⚠️ Known Issues
- None at this time

### 📖 Documentation
- [README.md](README.md) - Setup and build instructions
- [FLASHING.md](FLASHING.md) - Mass production flashing guide
- [CONTRIBUTING.md](CONTRIBUTING.md) - Developer guide
```

## Post-Release

- [ ] Download and verify release artifacts work
- [ ] Test flash on clean badge
- [ ] Update any external documentation
- [ ] Monitor for bug reports
- [ ] Plan next release features

## Hotfix Process

For critical bugs requiring immediate fix:

```bash
# Create hotfix branch from tag
git checkout v1.0.0
git checkout -b hotfix/critical-bug

# Fix the bug
# ... make changes ...

git commit -m "fix: resolve critical boot issue"
git push origin hotfix/critical-bug

# Create PR to main
# After merge, create patch release
git checkout main
git pull
git tag v1.0.1
git push origin v1.0.1
```

## Rollback

If a release has critical issues:

1. Mark release as "Pre-release" in GitHub
2. Add warning to release notes
3. Point users to previous stable version
4. Create hotfix as above

## Archive Old Releases

Keep at least 3 most recent releases available. Archive older releases:
1. Edit release → Check "Set as a pre-release"
2. Update description: "Archived - use v1.x.x instead"
