# Point every Start Menu / Desktop "Dogecoin Core Pro" shortcut at corepro-launch.exe
# (WIN32 picker / GUI). Run as the installing user; prefers all-users folders.
$ErrorActionPreference = "Continue"
$inst = "C:\Program Files\Dogecoin"
$launch = Join-Path $inst "corepro-launch.exe"
if (-not (Test-Path $launch)) { throw "Missing $launch" }
$wshell = New-Object -ComObject WScript.Shell

$testnetIco = Join-Path $inst "dogecoin-testnet.ico"
if (-not (Test-Path $testnetIco)) {
  $alt = "C:\dogedev\share\pixmaps\dogecoin-testnet.ico"
  if (Test-Path $alt) { $testnetIco = $alt }
}

function Write-Lnk([string]$path, [string]$lnkArgs = "", [string]$desc = "Dogecoin Core Pro") {
  $dir = Split-Path $path -Parent
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  $s = $wshell.CreateShortcut($path)
  $s.TargetPath = $launch
  $s.Arguments = $lnkArgs
  $s.WorkingDirectory = $inst
  $s.WindowStyle = 1
  if ($lnkArgs -match "testnet" -and (Test-Path $testnetIco)) {
    $s.IconLocation = "$testnetIco,0"
  } else {
    $s.IconLocation = "$launch,0"
  }
  $s.Description = $desc
  $s.Save()
  Write-Host "SHORTCUT $path -> $launch $lnkArgs"
}

$names = @("Dogecoin Core Pro.lnk", "Dogecoin Core.lnk")
$folders = @(
  "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\Dogecoin Core Pro",
  "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Dogecoin Core Pro",
  "$env:PUBLIC\Desktop",
  [Environment]::GetFolderPath("Desktop"),
  [Environment]::GetFolderPath("CommonDesktopDirectory")
) | Where-Object { $_ }

foreach ($folder in $folders) {
  foreach ($n in $names) {
    $p = Join-Path $folder $n
    if ((Test-Path $p) -or $folder -match "Start Menu" -or $n -eq "Dogecoin Core Pro.lnk") {
      if ($folder -match "Start Menu" -or $n -eq "Dogecoin Core Pro.lnk") {
        Write-Lnk $p
      }
    }
  }
  if ($folder -match "Start Menu" -or $folder -match "Desktop") {
    Write-Lnk (Join-Path $folder "Dogecoin Core Pro Testnet.lnk") "--testnet" "Dogecoin Core Pro (testnet)"
  }
}

# Drop leftover names that skip the launcher, plus the extra tray helper
# (gpenode-tray is a second notify icon; the GUI owns the one tray).
foreach ($folder in $folders) {
  foreach ($n in @("Desktop GUI.lnk", "Choose UI (ask).lnk", "Hybrid tray.lnk")) {
    $p = Join-Path $folder $n
    if (Test-Path $p) {
      Remove-Item $p -Force
      Write-Host "REMOVED $p"
    }
  }
}
$startupFolders = @(
  "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup",
  "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\Startup"
)
foreach ($folder in $startupFolders) {
  $p = Join-Path $folder "Dogecoin Core Pro tray.lnk"
  if (Test-Path $p) {
    Remove-Item $p -Force
    Write-Host "REMOVED $p"
  }
}
Get-Process -Name "gpenode-tray" -ErrorAction SilentlyContinue | ForEach-Object {
  Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
  Write-Host "STOPPED gpenode-tray pid=$($_.Id)"
}
Write-Host "SHORTCUTS_OK"
