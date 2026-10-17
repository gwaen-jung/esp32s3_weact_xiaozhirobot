param(
    [switch]$SyncBoard
)

$ErrorActionPreference = "Stop"

if ($SyncBoard) {
    Write-Host "==> Dong bo firmware/esp32s3-weact-dog vao xiaozhi-esp32..." -ForegroundColor Cyan
    Copy-Item -Path "$PSScriptRoot\firmware\esp32s3-weact-dog\*" -Destination "$PSScriptRoot\xiaozhi-esp32\main\boards\esp32s3-weact-dog\" -Recurse -Force
}

Set-Location "$PSScriptRoot\xiaozhi-esp32"
powershell -File .\build_dog.ps1
Set-Location $PSScriptRoot
