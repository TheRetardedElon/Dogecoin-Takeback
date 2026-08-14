# Repair an already-installed Hybrid: delete Qt, fix Start Menu, install launcher pref.
# Run elevated. Does not touch dogecoind / datadir / wallet.
$ErrorActionPreference = "Stop"
$inst = "C:\Program Files\Dogecoin"
if (-not (Test-Path $inst)) {
  $inst = "${env:ProgramFiles}\Dogecoin"
}
if (-not (Test-Path $inst)) { throw "Install dir not found: $inst" }

$src = Split-Path -Parent $MyInvocation.MyCommand.Path
Copy-Item -Force (Join-Path $src "launch-hybrid.ps1") (Join-Path $inst "launch-hybrid.ps1")

$pref = Join-Path $inst "hybrid-ui.txt"
if (-not (Test-Path $pref)) {
  Set-Content -Path $pref -Value "ask" -Encoding ASCII
}

foreach ($n in @("dogecoin-qt.exe", "dogecoin-qt")) {
  $p = Join-Path $inst $n
  if (Test-Path $p) {
    try { Remove-Item -Force $p; Write-Host "DELETED $p" }
    catch { Write-Host "LOCKED $p - close Qt and delete it" }
  }
}

$programs = [Environment]::GetFolderPath("CommonPrograms")
$sm = Join-Path $programs "Dogecoin Core Pro"
if (-not (Test-Path $sm)) { $sm = Join-Path $programs "Dogecoin Core" }
if (-not (Test-Path $sm)) { New-Item -ItemType Directory -Force -Path (Join-Path $programs "Dogecoin Core Pro") | Out-Null; $sm = Join-Path $programs "Dogecoin Core Pro" }

$ps = "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
$launchArgs = "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$inst\launch-hybrid.ps1`""
$askArgs = "$launchArgs -ForceAsk"

function New-Lnk([string]$path, [string]$target, [string]$args, [string]$workdir) {
  $w = New-Object -ComObject WScript.Shell
  $s = $w.CreateShortcut($path)
  $s.TargetPath = $target
  $s.Arguments = $args
  $s.WorkingDirectory = $workdir
  $s.Save()
}

New-Lnk (Join-Path $sm "Dogecoin Core Pro.lnk") $ps $launchArgs $inst
New-Lnk (Join-Path $sm "Dogecoin Core.lnk") $ps $launchArgs $inst
foreach ($n in @("Desktop GUI.lnk", "Operator TUI.lnk", "Choose UI (ask).lnk")) {
  $p = Join-Path $sm $n
  if (Test-Path $p) { Remove-Item -Force $p }
}

$desk = [Environment]::GetFolderPath("CommonDesktopDirectory")
New-Lnk (Join-Path $desk "Dogecoin Core Pro.lnk") $ps $launchArgs $inst

foreach ($old in @(
  (Join-Path $sm "Dogecoin Core (64-bit).lnk"),
  (Join-Path $programs "Dogecoin Core\Dogecoin Core.lnk"),
  (Join-Path $programs "Dogecoin Core\Dogecoin Core (64-bit).lnk"),
  (Join-Path ([Environment]::GetFolderPath("Desktop")) "Dogecoin Core.lnk")
)) {
  if (Test-Path $old) { Remove-Item -Force $old; Write-Host "REMOVED shortcut $old" }
}

Write-Host "REPAIR_OK $inst"
Get-ChildItem $inst -Name | Sort-Object
