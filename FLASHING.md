# Badge Flashing Quick Reference

## For Mass Production / Distribution

### Requirements

- Python 3.7+ with esptool: `python -m pip install esptool`
- USB-C cable
- Badge firmware files from GitHub release

### Batch Flashing Script

**Windows (`flash.bat`):**
```batch
@echo off
set PORT=COM4
set BAUD=460800

where python > NUL 2>&1
if %errorlevel% NEQ 0 (
  echo ERROR: Python not found in PATH!
  pause
  exit /b 1
)

echo Flashing NorthSec Badge 2026...
python -m esptool --chip esp32s3 --port %PORT% --baud %BAUD% ^
  --before default_reset --after hard_reset write_flash -z ^
  --flash_mode dio --flash_freq 80m --flash_size 8MB ^
  0x0 bootloader.bin ^
  0x8000 partitions.bin ^
  0x10000 badge-factory.bin ^
  0x150000 badge-ota.bin

if %errorlevel% equ 0 (
  echo SUCCESS: Badge flashed successfully!
) else (
  echo ERROR: Flashing failed!
  pause
  exit /b 1
)
pause
```

**Windows (`flash.ps1`):**
```powershell
$PORT = "COM4"
$BAUD = 460800

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: Python not found in PATH!"
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "Flashing NorthSec Badge 2026..."

python -m esptool --chip esp32s3 --port $PORT --baud $BAUD `
    --before default_reset --after hard_reset write_flash -z `
    --flash_mode dio --flash_freq 80m --flash_size 8MB `
    0x0 bootloader.bin `
    0x8000 partitions.bin `
    0x10000 badge-factory.bin `
    0x150000 badge-ota.bin

if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: Badge flashed successfully!"
} else {
    Write-Host "ERROR: Flashing failed!"
    Read-Host "Press Enter to exit"
    exit 1
}

Read-Host "Press Enter to exit"
```

**Linux/Mac (`flash.sh`):**
```bash
#!/bin/bash
PORT=/dev/ttyACM0
BAUD=460800

which python > /dev/null 2>&1
if [ $? -neq 0 ]; then
  echo "ERROR: Flashing failed!"
  exit 1
fi

echo "Flashing NorthSec Badge 2026..."
python3 -m esptool --chip esp32s3 --port $PORT --baud $BAUD \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 badge-factory.bin \
  0x150000 badge-ota.bin

if [ $? -eq 0 ]; then
    echo "SUCCESS: Badge flashed successfully!"
else
    echo "ERROR: Flashing failed!"
    exit 1
fi
```

### Memory Map Reference

| Address    | Size      | Content              | Purpose                           |
|------------|-----------|----------------------|-----------------------------------|
| 0x0        | ~15KB     | bootloader.bin       | ESP32-S3 second-stage bootloader  |
| 0x8000     | 3KB       | partitions.bin       | Partition table                   |
| 0xE000     | 8KB       | (auto)               | OTA data selector                 |
| 0x10000    | 1.25MB    | badge-factory.bin    | Conference firmware (factory)     |
| 0x150000   | 1.25MB    | badge-ota.bin        | CTF challenges (ota_0)            |
| 0x290000   | 64KB      | (reserved)           | Core dump partition               |
| 0x2A0000   | ~1.4MB    | (empty)              | SPIFFS filesystem                 |

### Verification Checklist

After flashing each badge:

1. **Connect to serial** (115200 baud) - Should see boot banner
2. **Check factory boot**: Device should boot to conference firmware by default
3. **Test CLI**: Type `help` - should show commands including `schedule`
4. **Test OTA switch**: Type `boot ota` - device reboots to challenges firmware
5. **Verify OTA boot**: Type `help` - should show commands including `crypto`
6. **Test return**: Type `boot factory` - returns to conference firmware

### Troubleshooting

**"Failed to connect":**
- Hold BOOT button while connecting USB
- Try lower baud rate: `--baud 115200`
- Check USB cable (must support data, not just charging)

**"Hash of data verified" but badge not working:**
- Erase flash first: `python -m esptool --chip esp32s3 --port <PORT> erase_flash`
- Reflash with all files

**Wrong partition boots:**
- Flash OTA data partition: `python -m esptool --chip esp32s3 --port <PORT> write_flash 0xE000 ota_data_initial.bin`
- Or use CLI: `boot factory` then `reboot`

### Quality Assurance

Test sample from each batch:
- [ ] Boot banner displays correctly
- [ ] USB CDC serial communication working
- [ ] LED blinks (heartbeat)
- [ ] CLI responsive to commands
- [ ] Both firmware partitions functional
- [ ] Boot switching works in both directions

### Support

For flashing issues or firmware bugs, open an issue at:
https://github.com/nsec/badge-2026/issues
