param(
    [string]$Port = "COM18"
)

$ErrorActionPreference = "Stop"

$pythonExe = "C:\Users\trung\.platformio\penv\Scripts\python.exe"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Flashing [env:test_sensors] to $Port..." -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan

& $pythonExe -m esptool --chip esp32s3 -p $Port -b 460800 write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 .pio\build\test_sensors\bootloader.bin 0x8000 .pio\build\test_sensors\partitions.bin 0x20000 .pio\build\test_sensors\firmware.bin

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  FLASH THANH CONG MOI TRUONG TEST SENSORS HUD!" -ForegroundColor Green
Write-Host "  Man hinh LCD 2.4 inch dang hien thi giao dien HUD do khoang cach Laser ToF va goc nghieng MPU6050." -ForegroundColor Yellow
Write-Host "  Serial Monitor: 115200 baud" -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan
