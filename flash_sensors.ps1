param(
    [string]$Port = "COM18",
    [switch]$SkipBuild
)

# Build + flash [env:test_sensors]. Dung chung logic voi flash_servo.ps1
# (sua loi cu: firmware truoc day bi ghi nham o 0x20000 thay vi app0 = 0x10000).
& (Join-Path $PSScriptRoot "flash_servo.ps1") -Port $Port -EnvName "test_sensors" -SkipBuild:$SkipBuild
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "  Man hinh LCD hien HUD khoang cach Laser ToF va goc nghieng MPU6050." -ForegroundColor Yellow
