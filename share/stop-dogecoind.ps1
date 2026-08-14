# Stop the Core Pro / GPENode dogecoind so uninstall can remove files.
# Prefer service + RPC stop (flush). Last resort: taskkill after wait.
param(
  [int]$WaitSeconds = 90,
  [string]$DataDir = "",
  [string]$ServiceName = "DogecoinGPENode"
)
$ErrorActionPreference = "Continue"

function Test-Doge {
  return [bool](Get-Process -Name "dogecoind" -ErrorAction SilentlyContinue)
}

if (-not $DataDir) {
  if ($env:APPDATA) { $DataDir = Join-Path $env:APPDATA "Dogecoin" }
}

Write-Host "STOP_BEGIN service=$ServiceName datadir=$DataDir"

$svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($svc) {
  if ($svc.Status -ne "Stopped") {
    Write-Host "SERVICE_STOP $ServiceName"
    Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
    & sc.exe stop $ServiceName | Out-Null
  }
}

# RPC stop so wallet/chainstate flush (uses installer dogecoin.conf)
$cliCands = @(
  (Join-Path $PSScriptRoot "daemon\dogecoin-cli.exe"),
  (Join-Path $PSScriptRoot "dogecoin-cli.exe"),
  (Join-Path $PSScriptRoot "bin\dogecoin-cli.exe")
)
$cli = $cliCands | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($cli -and (Test-Path (Join-Path $DataDir "dogecoin.conf"))) {
  Write-Host "RPC_STOP $cli -datadir=$DataDir"
  & $cli "-datadir=$DataDir" stop 2>$null | Out-Null
}

$deadline = (Get-Date).AddSeconds($WaitSeconds)
while ((Test-Doge) -and (Get-Date) -lt $deadline) {
  Start-Sleep -Seconds 2
  Write-Host "WAIT dogecoind still running..."
}

if (Test-Doge) {
  Write-Host "TASKKILL leftover dogecoind (uninstall cannot leave it running)"
  Stop-Process -Name "dogecoind" -Force -ErrorAction SilentlyContinue
  Start-Sleep -Seconds 2
}

if ($svc) {
  & sc.exe delete $ServiceName | Out-Null
}

if (Test-Doge) {
  Write-Host "STOP_FAIL dogecoind still running"
  exit 1
}
Write-Host "STOP_OK"
exit 0
