# Hybrid launcher: one dogecoind, pick ImGui or operator TUI.
# Preference file (ask | gfx | tui) lives next to this script: hybrid-ui.txt
# Does not start a second daemon if one is already running.
# Never launches dogecoin-qt.
# If Start Menu launches us Hidden, WinForms never paints — relaunch visible for the picker.
param(
  [ValidateSet("ask", "gfx", "tui", "")]
  [string]$Ui = "",
  [switch]$ForceAsk,
  [ValidateSet("ask", "gfx", "tui")]
  [string]$SetDefault = "",
  [switch]$PickerVisible
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$prefFile = Join-Path $here "hybrid-ui.txt"
$crashLog = Join-Path $env:TEMP "dogecoin-core-pro-launch.log"

function Show-LaunchFail([string]$msg) {
  try {
    Add-Type -AssemblyName System.Windows.Forms | Out-Null
    [System.Windows.Forms.MessageBox]::Show($msg, "Dogecoin Core Pro", "OK", "Error") | Out-Null
  } catch {
    $msg | Set-Content -Path $crashLog -Encoding utf8
  }
}

function Normalize-Ui([string]$v) {
  $v = ($v | Out-String).Trim().ToLowerInvariant()
  switch ($v) {
    { $_ -in @("gfx", "gui", "imgui", "desktop") } { return "gfx" }
    { $_ -in @("tui", "operator", "gpenode") } { return "tui" }
    { $_ -in @("ask", "prompt", "choose") } { return "ask" }
    default { return "" }
  }
}

function Read-HybridPref {
  if (Test-Path $prefFile) {
    foreach ($line in Get-Content -Path $prefFile -ErrorAction SilentlyContinue) {
      $n = Normalize-Ui $line
      if ($n) { return $n }
    }
  }
  $app = Join-Path $env:APPDATA "Dogecoin\hybrid-ui.txt"
  if (Test-Path $app) {
    foreach ($line in Get-Content -Path $app -ErrorAction SilentlyContinue) {
      $n = Normalize-Ui $line
      if ($n) { return $n }
    }
  }
  return "ask"
}

function Write-HybridPref([string]$value) {
  $value = Normalize-Ui $value
  if (-not $value) { $value = "ask" }
  $body = @"
# Hybrid default UI for this install. ask | gfx | tui
# ask  = prompt every time
# gfx  = Desktop GUI (ImGui)
# tui  = Operator TUI
$value
"@
  $utf8 = New-Object System.Text.UTF8Encoding $false
  [System.IO.File]::WriteAllText($prefFile, $body, $utf8)
}

function Test-Dogecoind {
  return [bool](Get-Process -Name "dogecoind" -ErrorAction SilentlyContinue)
}

function Find-Bin([string]$Name) {
  $cands = @(
    (Join-Path $here $Name),
    (Join-Path $here "bin\$Name"),
    (Join-Path $here "daemon\$Name")
  )
  foreach ($p in $cands) {
    if (Test-Path $p) { return $p }
  }
  return $null
}

if ($SetDefault) {
  Write-HybridPref $SetDefault
  Write-Host "HYBRID_UI $($SetDefault.ToLowerInvariant())"
  exit 0
}

$gui = Find-Bin "dogecoin-pro-gui.exe"
if (-not $gui) { $gui = Find-Bin "dogecoin-pro-gui-smoke.exe" }
$tui = Find-Bin "gpenode-tui.exe"
$doge = Find-Bin "dogecoind.exe"

$requested = Normalize-Ui $Ui
if ($ForceAsk) { $requested = "ask" }
if (-not $requested) { $requested = Read-HybridPref }
if (-not $requested) { $requested = "ask" }

function Hide-OwnConsole {
  try {
    Add-Type -Namespace CorePro -Name Native -MemberDefinition @'
[DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
'@ -ErrorAction SilentlyContinue
    $h = [CorePro.Native]::GetConsoleWindow()
    if ($h -ne [IntPtr]::Zero) { [CorePro.Native]::ShowWindow($h, 0) | Out-Null }
  } catch {}
}

function Show-HybridPicker {
  Add-Type -AssemblyName System.Windows.Forms | Out-Null
  Add-Type -AssemblyName System.Drawing | Out-Null

  $form = New-Object System.Windows.Forms.Form
  $form.Text = "Dogecoin Core Pro  -  Hybrid"
  $form.Size = New-Object System.Drawing.Size(500, 340)
  $form.StartPosition = "CenterScreen"
  $form.FormBorderStyle = "FixedDialog"
  $form.MaximizeBox = $false
  $form.MinimizeBox = $false
  $form.TopMost = $true
  $form.ShowInTaskbar = $true
  $form.StartPosition = "CenterScreen"

  $lbl = New-Object System.Windows.Forms.Label
  $lbl.Location = New-Object System.Drawing.Point(16, 12)
  $lbl.Size = New-Object System.Drawing.Size(460, 56)
  $lbl.Text = "Hybrid = this PC is a full Dogecoin node (a server), plus a desktop wallet. You are not a light client. One dogecoind. Which UI?"
  $form.Controls.Add($lbl)

  $rbGui = New-Object System.Windows.Forms.RadioButton
  $rbGui.Location = New-Object System.Drawing.Point(24, 76)
  $rbGui.Size = New-Object System.Drawing.Size(440, 24)
  $rbGui.Text = "Desktop GUI  (wallet on this server node)"
  $rbGui.Checked = $true
  $form.Controls.Add($rbGui)

  $rbTui = New-Object System.Windows.Forms.RadioButton
  $rbTui.Location = New-Object System.Drawing.Point(24, 104)
  $rbTui.Size = New-Object System.Drawing.Size(440, 24)
  $rbTui.Text = "Operator TUI  (server control: service, dump, CDN)"
  $form.Controls.Add($rbTui)

  $chk = New-Object System.Windows.Forms.CheckBox
  $chk.Location = New-Object System.Drawing.Point(24, 142)
  $chk.Size = New-Object System.Drawing.Size(440, 24)
  $chk.Text = "Remember this  (do not ask next time)"
  $form.Controls.Add($chk)

  $hint = New-Object System.Windows.Forms.Label
  $hint.Location = New-Object System.Drawing.Point(16, 174)
  $hint.Size = New-Object System.Drawing.Size(460, 48)
  $hint.Text = "X sends the UI to the system tray — the node stays up. Tray can open Desktop GUI or Operator TUI. Change later in Options > Hybrid or TUI Settings (H)."
  $form.Controls.Add($hint)

  $ok = New-Object System.Windows.Forms.Button
  $ok.Text = "Open"
  $ok.Location = New-Object System.Drawing.Point(170, 250)
  $ok.Size = New-Object System.Drawing.Size(88, 28)
  $ok.DialogResult = [System.Windows.Forms.DialogResult]::OK
  $form.AcceptButton = $ok
  $form.Controls.Add($ok)

  $cancel = New-Object System.Windows.Forms.Button
  $cancel.Text = "Cancel"
  $cancel.Location = New-Object System.Drawing.Point(270, 250)
  $cancel.Size = New-Object System.Drawing.Size(88, 28)
  $cancel.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
  $form.CancelButton = $cancel
  $form.Controls.Add($cancel)

  $r = $form.ShowDialog()
  if ($r -ne [System.Windows.Forms.DialogResult]::OK) { return $null }
  $pick = "gfx"
  if ($rbTui.Checked) { $pick = "tui" }
  if ($chk.Checked) { Write-HybridPref $pick }
  return $pick
}

if ($requested -eq "ask") {
  # Start Menu uses -WindowStyle Hidden. A WinForms dialog owned by a hidden
  # host never appears — user clicks the icon and nothing opens.
  if (-not $PickerVisible) {
    $self = $MyInvocation.MyCommand.Path
    Start-Process -FilePath "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" `
      -ArgumentList @("-NoProfile","-STA","-ExecutionPolicy","Bypass","-File",$self,"-PickerVisible") `
      -WorkingDirectory $here -WindowStyle Normal
    exit 0
  }
  Hide-OwnConsole
  try {
    $picked = Show-HybridPicker
  } catch {
    Show-LaunchFail "Hybrid picker failed:`n$($_.Exception.Message)`nOpening Desktop GUI."
    $picked = "gfx"
  }
  if (-not $picked) { exit 0 }
  $requested = $picked
}

if (-not (Test-Dogecoind) -and $doge) {
  Start-Process -FilePath $doge -ArgumentList "-server" -WorkingDirectory (Split-Path $doge) -WindowStyle Hidden
  Start-Sleep -Seconds 2
}

try {
  if ($requested -eq "gfx") {
    if (-not $gui) { throw "Desktop GUI not installed (dogecoin-pro-gui.exe missing)." }
    Start-Process -FilePath $gui -ArgumentList "--ui","gfx" -WorkingDirectory (Split-Path $gui)
    exit 0
  }

  if ($requested -eq "tui") {
    if (-not $tui) { throw "Operator TUI not installed (gpenode-tui.exe missing)." }
    Start-Process -FilePath $tui -ArgumentList "--ui","tui" -WorkingDirectory (Split-Path $tui)
    exit 0
  }

  throw "Unknown hybrid UI '$requested'"
} catch {
  Show-LaunchFail $_.Exception.Message
  exit 1
}
