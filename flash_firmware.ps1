param(
    [string]$Port = "COM19",
    [int]$Baud = 460800,
    [switch]$AppOnly,
    [switch]$FullMerged,
    [switch]$Monitor
)

$ErrorActionPreference = "Stop"

$python = "C:\Users\trung\.platformio\penv\Scripts\python.exe"
$buildDir = "$PSScriptRoot\xiaozhi-esp32\build"

if (-not (Test-Path "$buildDir\xiaozhi.bin")) {
    throw "Khong tim thay firmware trong $buildDir. Hay chay .\build_firmware.ps1 truoc!"
}

if ($AppOnly) {
    Write-Host "==> Nap rieng app xiaozhi.bin len $Port tai offset 0x20000..." -ForegroundColor Green
    & $python -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x20000 "$buildDir\xiaozhi.bin"
} elseif ($FullMerged) {
    Write-Host "==> Nap merged-binary.bin len $Port tai offset 0x0..." -ForegroundColor Green
    & $python -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 "$buildDir\merged-binary.bin"
} else {
    Write-Host "==> Nap toan bo firmware (bootloader, partitions, assets, app) len $Port..." -ForegroundColor Green
    Set-Location $buildDir
    & $python -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset write-flash "@flash_args"
    Set-Location $PSScriptRoot
}

if ($LASTEXITCODE -ne 0) {
    throw "Nap firmware that bai!"
}

Write-Host "==> Nap firmware thanh cong vao ESP32-S3 ($Port)!" -ForegroundColor Green

if ($Monitor) {
    Write-Host "==> Mo Serial Monitor tren $Port (115200)... (Nhan Ctrl+] de thoat)" -ForegroundColor Cyan
    & $python -m serial.tools.miniterm --raw $Port 115200
}
