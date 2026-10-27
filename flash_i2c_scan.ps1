param(
    [string]$Port = "",
    [switch]$SkipBuild,
    [switch]$NoMonitor
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$pioHome   = Join-Path $env:USERPROFILE ".platformio"
$pythonExe = Join-Path $pioHome "penv\Scripts\python.exe"
$pioExe    = Join-Path $pioHome "penv\Scripts\pio.exe"

# Tự động dò tìm cổng COM nếu chưa truyền vào
if (-not $Port) {
    Write-Host "Dang tim kiem cong COM ESP32-S3..." -ForegroundColor Cyan
    $ports = & $pythonExe -c "import serial.tools.list_ports as lp; print(';'.join([p.device for p in lp.comports()]))"
    if ($ports) {
        $portList = $ports.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries)
        if ($portList.Count -eq 1) {
            $Port = $portList[0]
            Write-Host "-> Tu dong chon cong: $Port" -ForegroundColor Green
        } else {
            Write-Host "Tim thay cac cong: $($portList -join ', ')" -ForegroundColor Yellow
            $Port = $portList[0]
            Write-Host "-> Mac dinh chon cong dau tien: $Port" -ForegroundColor Green
        }
    } else {
        $Port = "COM18" # fallback
        Write-Host "Chua thay cong COM nao duoc cam! Thu voi $Port..." -ForegroundColor Yellow
    }
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  FLASHING & RUNNING I2C SCANNER (GPIO 41 & 42) -> $Port"   -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

# Gọi flash_servo.ps1 để nạp firmware
& (Join-Path $PSScriptRoot "flash_servo.ps1") -Port $Port -EnvName "test_i2c_scan" -SkipBuild:$SkipBuild

if (-not $NoMonitor) {
    Write-Host "`n==========================================================" -ForegroundColor Cyan
    Write-Host "  DANG MO SERIAL MONITOR (115200) DE XEM KET QUA QUET I2C"   -ForegroundColor Green
    Write-Host "  (Nhan Ctrl+C hoac Ctrl+] de thoat Monitor)"                -ForegroundColor Yellow
    Write-Host "==========================================================`n" -ForegroundColor Cyan
    & $pioExe device monitor -p $Port -b 115200
}
