param(
    [string]$Port = "COM18",
    [string]$EnvName = "test_servo",
    [switch]$SkipBuild
)

# Build + flash 1 env PlatformIO len WeAct ESP32-S3 qua CH343.
# Dung chung cho moi env:  .\flash_servo.ps1 -EnvName test_sensors -Port COM18

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$pioHome   = Join-Path $env:USERPROFILE ".platformio"
$pythonExe = Join-Path $pioHome "penv\Scripts\python.exe"
$pioExe    = Join-Path $pioHome "penv\Scripts\pio.exe"
$bootApp0  = Join-Path $pioHome "packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
$buildDir  = ".pio\build\$EnvName"

foreach ($f in @($pythonExe, $pioExe, $bootApp0)) {
    if (-not (Test-Path $f)) { throw "Khong tim thay: $f (da cai PlatformIO chua?)" }
}

if (-not $SkipBuild) {
    Write-Host "==> Build [env:$EnvName]..." -ForegroundColor Cyan
    & $pioExe run -e $EnvName
    if ($LASTEXITCODE -ne 0) { throw "Build that bai." }
}

Write-Host "==> Dua ESP32-S3 vao Download Mode qua $Port..." -ForegroundColor Yellow
& $pythonExe -c @"
import serial, time
s = serial.Serial('$Port', 115200, timeout=1)
s.dtr = False; s.rts = True;  time.sleep(0.1)   # EN = LOW (reset)
s.dtr = True;  s.rts = False; time.sleep(0.1)   # IO0 = LOW, EN = HIGH
s.dtr = False; time.sleep(0.2)
s.close()
"@
Start-Sleep -Milliseconds 200

# Dia chi theo default_16MB.csv: bootloader 0x0, partitions 0x8000,
# otadata 0xe000 (boot_app0 -> xoa trang thai OTA cua firmware cu), app0 0x10000.
Write-Host "==> Flash [env:$EnvName] len $Port..." -ForegroundColor Green
& $pythonExe -m esptool --chip esp32s3 -p $Port -b 921600 --before no_reset --after hard_reset `
    write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m `
    0x0     "$buildDir\bootloader.bin" `
    0x8000  "$buildDir\partitions.bin" `
    0xe000  "$bootApp0" `
    0x10000 "$buildDir\firmware.bin"
if ($LASTEXITCODE -ne 0) { throw "Flash that bai." }

Write-Host "==> XONG. Serial Monitor: pio device monitor -p $Port -b 115200" -ForegroundColor Cyan
