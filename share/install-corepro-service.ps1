#Requires -RunAsAdministrator
# Register Dogecoin Core Pro as a Windows service.
# dogecoind is not a native SCM binary; gpenode-ops service-run is the host.
# Service key stays DogecoinGPENode (gpenode-ops svc.Run name). Display name is Core Pro.
# Does not start the service if dogecoind is already running (one node only).
param(
  [string]$BinDir = "",
  [string]$OpsPath = "",
  [string]$DataDir = "",
  [string]$ConfFile = "",
  [string]$ServiceName = "DogecoinGPENode",
  [string]$DisplayName = "Dogecoin Core Pro",
  [switch]$StartIfIdle
)
$ErrorActionPreference = "Stop"

function Find-Exe([string]$Dir, [string]$Name) {
  foreach ($p in @(
    (Join-Path $Dir $Name),
    (Join-Path $Dir "daemon\$Name"),
    (Join-Path $Dir "bin\$Name"),
    (Join-Path (Split-Path $Dir -Parent) $Name)
  )) {
    if (Test-Path $p) { return [IO.Path]::GetFullPath($p) }
  }
  return $null
}

if (-not $BinDir) { $BinDir = $PSScriptRoot }
$BinDir = [IO.Path]::GetFullPath($BinDir)

$doge = Find-Exe $BinDir "dogecoind.exe"
$cli = Find-Exe $BinDir "dogecoin-cli.exe"
if (-not $OpsPath) { $OpsPath = Find-Exe $BinDir "gpenode-ops.exe" }
if (-not $OpsPath) { throw "gpenode-ops.exe not found (service host)" }
if (-not $doge) { throw "dogecoind.exe not found under $BinDir" }
$OpsPath = [IO.Path]::GetFullPath($OpsPath)

if (-not $DataDir) {
  $role = ""
  $rf = Join-Path $PSScriptRoot "install-role.txt"
  if (Test-Path $rf) { $role = (Get-Content $rf -Raw).Trim().ToLowerInvariant() }
  if ($role -eq "server") {
    $DataDir = Join-Path $env:ProgramData "DogecoinGPENode"
  } else {
    $DataDir = Join-Path $env:APPDATA "Dogecoin"
  }
}
$DataDir = [IO.Path]::GetFullPath($DataDir)
if (-not $ConfFile) { $ConfFile = Join-Path $DataDir "dogecoin.conf" }
$ConfFile = [IO.Path]::GetFullPath($ConfFile)

New-Item -ItemType Directory -Force -Path $DataDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $DataDir "logs") | Out-Null

if (-not (Test-Path $ConfFile)) {
  throw "Missing $ConfFile - run the installer first (unique RPC password)."
}

$existing = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($existing) {
  if ($existing.Status -eq "Running") {
    Write-Host "SERVICE_EXISTS_RUNNING $ServiceName"
    exit 0
  }
  Stop-Service $ServiceName -Force -ErrorAction SilentlyContinue
  Start-Sleep -Seconds 2
  & sc.exe delete $ServiceName | Out-Null
  Start-Sleep -Seconds 2
}

$binPath = '"{0}" service-run -dogecoind="{1}" -datadir="{2}" -conf="{3}"' -f $OpsPath, $doge, $DataDir, $ConfFile
if ($cli) { $binPath += ' -cli="{0}"' -f $cli }

New-Service `
  -Name $ServiceName `
  -BinaryPathName $binPath `
  -DisplayName $DisplayName `
  -StartupType Automatic `
  -Description "Dogecoin Core Pro. One dogecoind. ImGui and TUI share this node. RPC localhost only." `
  | Out-Null

# Do not auto-restart on crash. A bad MDBX open would loop every few seconds
# and look like the node is being killed. The GUI starts the service instead.
& sc.exe failure $ServiceName reset= 86400 actions= ""/ | Out-Null
& sc.exe failureflag $ServiceName 0 | Out-Null
# Let the logged-on user start/stop from the GUI (no UAC). RP=start WP=stop LC=query.
$sddl = ((sc.exe sdshow $ServiceName 2>$null) | Out-String).Trim()
if ($sddl -match "^D:" -and $sddl -notmatch ";;;AU\)") {
  $new = $sddl -replace "^D:", "D:(A;;RPWPLC;;;AU)"
  & sc.exe sdset $ServiceName $new | Out-Null
}

$dogeRunning = [bool](Get-Process -Name "dogecoind" -ErrorAction SilentlyContinue)
if ($dogeRunning) {
  Write-Host "SERVICE_INSTALLED_NOT_STARTED dogecoind already running (one node). Service will take over on next boot or after you stop the current process."
  exit 0
}

if ($StartIfIdle) {
  try {
    Start-Service $ServiceName
    Write-Host "SERVICE_STARTED $ServiceName"
  } catch {
    Write-Host "SERVICE_INSTALLED_START_FAILED $($_.Exception.Message)"
    exit 0
  }
} else {
  Write-Host "SERVICE_INSTALLED $ServiceName"
}
exit 0
