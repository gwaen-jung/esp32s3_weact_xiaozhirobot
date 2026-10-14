param(
    [string]$Port = "COM18"
)

$ErrorActionPreference = "Stop"

$pythonExe = "C:\Users\trung\.platformio\penv\Scripts\python.exe"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Putting WeAct ESP32-S3 into Download Mode via $Port..." -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan

& $pythonExe -c "
import serial, time
s = serial.Serial('$Port', 115200, timeout=1)
s.dtr = False
s.rts = True
time.sleep(0.1)
s.dtr = True
s.rts = False
time.sleep(0.1)
s.dtr = False
time.sleep(0.2)
s.close()
"

Start-Sleep -Milliseconds 200

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Flashing test_servo (Final Suite) to $Port..." -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

& $pythonExe -m esptool --chip esp32s3 -p $Port -b 921600 --before no-reset --after hard-reset write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 .pio\build\test_servo\bootloader.bin 0x8000 .pio\build\test_servo\partitions.bin 0x10000 .pio\build\test_servo\firmware.bin
