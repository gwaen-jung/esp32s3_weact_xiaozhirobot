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
Write-Host "  FLASHING XIAOZHI CUSTOM PCB TEST ALL -> $Port"            -ForegroundColor Green
Write-Host "  - OLED SSD1306 (GPIO 1, 2)"                                -ForegroundColor Green
Write-Host "  - LOA MAX98357A & MIC INMP441 (GPIO 11, 12, 13, 14)"       -ForegroundColor Green
Write-Host "  - DRIVER MOTOR MINI L298N (GPIO 4, 5, 6, 7)"               -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

# Gọi PlatformIO nạp firmware cho môi trường test_pcb_all
if (-not $SkipBuild) {
    Write-Host "`n>> Dang bien dich va nap firmware [test_pcb_all]..." -ForegroundColor Yellow
    & $pioExe run -e test_pcb_all --target upload --upload-port $Port
}

if (-not $NoMonitor) {
    Write-Host "`n==========================================================" -ForegroundColor Cyan
    Write-Host "  DANG MO SERIAL MONITOR (115200) DE DIEU KHIEN VA XEM LOG"   -ForegroundColor Green
    Write-Host "  * PHIM TAT DIEU KHIEN MOTOR:"                              -ForegroundColor Yellow
    Write-Host "    [W/S] : Tien / Lui ca 2 dong co"                          -ForegroundColor White
    Write-Host "    [A/D] : Quay Trai / Quay Phai"                            -ForegroundColor White
    Write-Host "    [Space/X]: DUNG KHAN CAP"                                 -ForegroundColor Red
    Write-Host "    [1/2/3]  : Test tu dong Motor A / Motor B / Full Demo"     -ForegroundColor Cyan
    Write-Host "  * PHIM TAT TEST AM THANH:"                                  -ForegroundColor Yellow
    Write-Host "    [T] : Phat nhac Mario test Loa"                           -ForegroundColor White
    Write-Host "    [B] : Phat am quet tan so Sweep (300Hz - 2.4kHz)"         -ForegroundColor White
    Write-Host "    [L] : Bat/Tat Loopback Mic -> Loa"                        -ForegroundColor White
    Write-Host "    [M] : Chuyen che do man hinh OLED"                        -ForegroundColor White
    Write-Host "  (Nhan Ctrl+C hoac Ctrl+] de thoat Monitor)"                -ForegroundColor Yellow
    Write-Host "==========================================================`n" -ForegroundColor Cyan
    & $pioExe device monitor -p $Port -b 115200
}
