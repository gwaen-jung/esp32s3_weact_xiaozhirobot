param(
    [string]$Port = "COM18",
    [int]$Baud = 460800,
    [switch]$FullMerged,
    [switch]$Monitor
)

$ErrorActionPreference = "Stop"

$python = "C:\Users\trung\.platformio\penv\Scripts\python.exe"
$buildDir = "$PSScriptRoot\xiaozhi-esp32\build"

if ($FullMerged) {
    $binPath = "$buildDir\merged-binary.bin"
    if (-not (Test-Path $binPath)) {
        throw "Khong tim thay $binPath. Hay chay .\build_firmware.ps1 truoc!"
    }
    Write-Host "==> Nap toan bo merged-binary.bin len $Port (offset 0x0)..." -ForegroundColor Green
    & $python -m esptool --chip esp32s3 -p $Port -b $Baud write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 $binPath
} else {
    $binPath = "$buildDir\xiaozhi.bin"
    if (-not (Test-Path $binPath)) {
        throw "Khong tim thay $binPath. Hay chay .\build_firmware.ps1 truoc!"
    }
    Write-Host "==> Nap firmware xiaozhi.bin len $Port (offset 0x20000)..." -ForegroundColor Green
    & $python -m esptool --chip esp32s3 -p $Port -b $Baud write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x20000 $binPath
}

if ($LASTEXITCODE -ne 0) {
    throw "Nap firmware that bai!"
}

Write-Host "==> Nap thanh cong!" -ForegroundColor Green

if ($Monitor) {
    Write-Host "==> Mo Serial Monitor tren $Port..." -ForegroundColor Cyan
    & $python -m serial.tools.miniterm --raw $Port 115200
}
