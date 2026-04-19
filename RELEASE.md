# Release Process

For maintainers creating firmware releases.

## Two Release Modes

Every release produces **two** packages:

| Package | Purpose | Contents |
|---------|---------|----------|
| `nsec2026-conference-only-vX.Y.Z.tar.gz` | Pre-CTF distribution | Conference firmware only, `swapboot` disabled |
| `nsec2026-dual-firmware-vX.Y.Z.tar.gz` | Full badge experience | Conference + CTF firmware, `swapboot` enabled |

## Build Environments

| PIO Environment | Build Flag | Used In |
|----------------|------------|---------|
| `esp32-s3-devkitc-1-conference` | `HAS_CONFERENCE=1` | Dual package |
| `esp32-s3-devkitc-1-conference-only` | `HAS_CONFERENCE=1 CONFERENCE_ONLY=1` | Conference-only package |
| `esp32-s3-devkitc-1-ctf` | `HAS_CHALLENGES=1` | Dual package |

## Pre-Release Testing

### Conference-Only
- [ ] Build succeeds: `pio run -e esp32-s3-devkitc-1-conference-only`
- [ ] Flash and verify boot banner shows `Mode: conference-only`
- [ ] CLI `help` does **not** show `swapboot`
- [ ] Validation passes: `python tools/validate_conference_only.py release/conference-only/`

### Dual Firmware
- [ ] Build succeeds: `pio run -e esp32-s3-devkitc-1-conference -e esp32-s3-devkitc-1-ctf`
- [ ] Conference boot banner shows `Mode: conference (dual)`
- [ ] `swapboot` switches to CTF firmware
- [ ] CTF boot banner shows `Mode: ctf`
- [ ] `swapboot` returns to conference firmware

### General
- [ ] All PRs merged to `main`
- [ ] No compilation warnings
- [ ] Memory usage acceptable

## Creating a Release

### Step 1: Build and Package Locally (Optional)

```bash
# Build all three environments
pio run -e esp32-s3-devkitc-1-conference-only
pio run -e esp32-s3-devkitc-1-conference
pio run -e esp32-s3-devkitc-1-ctf

# Package
python tools/package_release.py --version vX.Y.Z

# Validate conference-only package
python tools/validate_conference_only.py release/conference-only/
```

### Step 2: Tag and Push

```bash
git checkout main
git pull origin main
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

### Step 3: CI Builds Automatically

The GitHub Actions workflow will:
1. Build all three firmware environments
2. Package both release modes
3. Validate conference-only package (no CTF leaks)
4. Create GitHub Release with both archives

### Step 4: Verify Release

1. Go to Releases page
2. Verify two archives are attached:
   - `nsec2026-conference-only-vX.Y.Z.tar.gz`
   - `nsec2026-dual-firmware-vX.Y.Z.tar.gz`
3. Download and spot-check `manifest.json` in each

## Version Numbering

Semantic versioning: `vMAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes
- **MINOR**: New features (e.g. new challenge module)
- **PATCH**: Bug fixes

## CTF Content Safety

The conference-only release must **never** contain:
- CTF firmware binary (`badge-ctf.bin`)
- The `swapboot` CLI command
- Challenge code or strings (quantum, QAOA, VQE, etc.)
- Flag patterns (`NSEC{...}`)

**Guardrails in place:**
1. `CONFERENCE_ONLY` build flag compiles out `swapboot` and prevents `HAS_CHALLENGES`
2. `#error` in `badge_config.h` if both `CONFERENCE_ONLY` and `HAS_CHALLENGES` are set
3. `validate_conference_only.py` scans binary for forbidden patterns
4. CI runs validation on every build

## Adding New CTF Content

When adding new challenges:
1. Place code in `lib/challenges/<name>/`
2. Register in `lib/challenges/registry.cpp`
3. Update `tools/validate_conference_only.py` with new forbidden patterns if the challenge introduces distinctive strings
4. Verify conference-only validation still passes

## Hotfix Process

```bash
git checkout v1.0.0
git checkout -b hotfix/critical-bug
# ... fix ...
git commit -m "fix: resolve critical issue"
git push origin hotfix/critical-bug
# PR to main, then tag v1.0.1
```

## Rollback

1. Mark release as "Pre-release" in GitHub
2. Add warning to release notes
3. Point users to previous stable version
