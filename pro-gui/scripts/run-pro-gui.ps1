# Run or smoke-test dogecoin-pro-gui via WSL (WSLg display).
param(
    [switch]$Smoke,
    [switch]$Build,
    [int]$SmokeSec = 8
)

$ErrorActionPreference = "Stop"
# This script: pro-gui/scripts/run-pro-gui.ps1
$ProGui = Split-Path $PSScriptRoot -Parent
# C:\dogedev\pro-gui -> /mnt/c/dogedev/pro-gui
$drive = $ProGui.Substring(0, 1).ToLower()
$rest = $ProGui.Substring(2).Replace('\', '/')
$UnixPro = "/mnt/$drive$rest"

Write-Host "pro-gui: $ProGui"
Write-Host "wsl path: $UnixPro"

if ($Build) {
    wsl -e bash ($UnixPro + "/scripts/build-wsl.sh")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Smoke) {
    $cmd = "SMOKE_SEC=" + $SmokeSec + " bash " + $UnixPro + "/scripts/smoke-pro-gui.sh"
    wsl -e bash -c $cmd
    exit $LASTEXITCODE
}

$bin = Join-Path $ProGui "build\dogecoin-pro-gui"
if (-not (Test-Path $bin)) {
    Write-Host "Binary missing - building first..."
    wsl -e bash ($UnixPro + "/scripts/build-wsl.sh")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Launching dogecoin-pro-gui (close the window to exit)..."
$bashCmd = "cd " + $UnixPro + "/build; ./dogecoin-pro-gui"
wsl -e bash -lc $bashCmd
exit $LASTEXITCODE
