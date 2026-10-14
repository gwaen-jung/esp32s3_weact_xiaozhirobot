param(
    [string]$XiaozhiDir = "E:\Claude\xiaozhi-dog\xiaozhi-esp32",
    [string]$IdfPath = "C:\esp\v6.0.1\esp-idf",
    [string]$IdfToolsPath = "C:\Espressif\tools6",
    [string]$Port = "COM8",
    [switch]$SyncBoard,   # copy firmware\esp32s3-weact-dog into the xiaozhi tree first
    [switch]$SkipBuild,
    [switch]$EraseFlash   # also wipes saved Wi-Fi / activation
)

# Build + flash the Xiaozhi chatbot firmware (board esp32s3-weact-dog, vi-VN).
$ErrorActionPreference = "Stop"

if ($SyncBoard) {
    $dst = Join-Path $XiaozhiDir "main\boards\esp32s3-weact-dog"
    Write-Host "==> Copy board sources -> $dst" -ForegroundColor Cyan
    Copy-Item -Recurse -Force (Join-Path $PSScriptRoot "esp32s3-weact-dog\*") $dst
}

$env:IDF_TOOLS_PATH = $IdfToolsPath
. (Join-Path $IdfPath "export.ps1") | Out-Null
Set-Location $XiaozhiDir

if (-not $SkipBuild) {
    Write-Host "==> Build esp32s3-weact-dog (vi-VN)..." -ForegroundColor Cyan
    python scripts/build.py esp32s3-weact-dog --language vi-VN
    if ($LASTEXITCODE -ne 0) { throw "Build that bai." }
}

$targets = if ($EraseFlash) { @("erase-flash", "flash") } else { @("flash") }
Write-Host "==> Flash len $Port..." -ForegroundColor Green
idf.py -p $Port -b 921600 @targets
if ($LASTEXITCODE -ne 0) { throw "Flash that bai." }
Write-Host "==> XONG. Monitor: idf.py -p $Port monitor" -ForegroundColor Cyan
