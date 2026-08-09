# Product P1: stream-hash fetchassumeutxo smoke (local path, fail-closed + OK)
# Requires dogecoind/cli in smoke-run linked with snapshot_fetch (relink-fetchassumeutxo-pe.sh)
$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false
$Smoke = 'C:\dogedev\smoke-run'
$Data = 'C:\dogedev\smoke-run\regtest-fetchassumeutxo-data'
$Daemon = Join-Path $Smoke 'dogecoind.exe'
$Cli = Join-Path $Smoke 'dogecoin-cli.exe'
$DatadirArg = "-datadir=$Data"

function Invoke-Cli {
    param([Parameter(ValueFromRemainingArguments = $true)]$Rest)
    $argList = @('-regtest', $DatadirArg) + @($Rest)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Cli
    $psi.Arguments = ($argList | ForEach-Object {
        $s = "$_"
        if ($s -match '\s') { '"{0}"' -f $s } else { $s }
    }) -join ' '
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    $script:LastCliExit = $proc.ExitCode
    $script:LastCliOut = $stdout.Trim()
    $script:LastCliErr = $stderr.Trim()
    return $proc.ExitCode
}

Write-Host "=== fetchassumeutxo stream-hash smoke ==="
if (-not (Test-Path $Daemon)) { throw "missing $Daemon - run scripts/relink-fetchassumeutxo-pe.sh first" }
if (-not (Test-Path $Cli)) { throw "missing $Cli" }

# Gate: help must list fetchassumeutxo
$helpProbe = & $Cli -regtest -h 2>&1 | Out-String
# daemon not up yet; check strings on binary instead
$binStrings = [System.IO.File]::ReadAllBytes($Daemon)
# Use findstr on file via Select-String on strings if available
$hasRpc = $false
try {
    $raw = & findstr /C:"fetchassumeutxo" $Daemon 2>$null
    if ($LASTEXITCODE -eq 0 -or ($raw -and $raw.Length -gt 0)) { $hasRpc = $true }
} catch {}
if (-not $hasRpc) {
    # PowerShell Select-String on binary often works
    if (Select-String -Path $Daemon -Pattern "fetchassumeutxo" -Quiet -Encoding byte -ErrorAction SilentlyContinue) {
        $hasRpc = $true
    }
}
# Fallback: dogecoind -help after start

Get-Process dogecoind -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
Start-Sleep -Seconds 2
if (Test-Path $Data) { Remove-Item -Recurse -Force $Data }
New-Item -ItemType Directory -Path $Data | Out-Null

Write-Host "Starting dogecoind -regtest..."
$p = Start-Process -FilePath $Daemon -ArgumentList @(
    '-regtest', $DatadirArg, '-server=1', '-listen=0', '-dnsseed=0', '-upnp=0'
) -PassThru -WindowStyle Hidden

$ready = $false
for ($i = 0; $i -lt 90; $i++) {
    Start-Sleep -Seconds 1
    if ($p.HasExited) {
        $log = Join-Path $Data 'regtest\debug.log'
        if (Test-Path $log) { Get-Content $log -Tail 40 }
        throw "dogecoind exited early code=$($p.ExitCode)"
    }
    $code = Invoke-Cli getblockcount
    if ($code -eq 0) { $ready = $true; break }
}
if (-not $ready) { throw "dogecoind not ready" }

# Confirm RPC exists
$code = Invoke-Cli help fetchassumeutxo
if ($code -ne 0) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    throw "fetchassumeutxo RPC missing (binary not relinked?): $LastCliErr $LastCliOut"
}
Write-Host "help fetchassumeutxo OK"

# Artifact fixture
$fixtureDir = Join-Path $Data 'fixtures'
New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null
$srcFile = Join-Path $fixtureDir 'artifact.bin'
[System.IO.File]::WriteAllBytes($srcFile, [System.Text.Encoding]::ASCII.GetBytes('dogecoin-pro-p1-fetch-smoke'))

# SHA256 via .NET
$sha = [System.Security.Cryptography.SHA256]::Create()
$bytes = [System.IO.File]::ReadAllBytes($srcFile)
$hash = ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) -join ''
Write-Host "fixture sha256=$hash bytes=$($bytes.Length)"

$destOk = 'snapshots/ok.dat'
$destBad = 'snapshots/bad.dat'

# --- Fail closed: wrong hash ---
Write-Host "Test FAIL-CLOSED wrong hash..."
$wrong = '0000000000000000000000000000000000000000000000000000000000000001'
$code = Invoke-Cli fetchassumeutxo $srcFile $wrong $destBad
if ($code -eq 0) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    throw "expected fetchassumeutxo to fail on wrong hash"
}
Write-Host "  fail-closed OK (cli exit=$code)"
$badPath = Join-Path $Data "regtest\$destBad"
if (Test-Path $badPath) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    throw "bad dest file should have been deleted: $badPath"
}
Write-Host "  dest absent OK"

# --- Success path ---
Write-Host "Test OK path correct hash..."
$code = Invoke-Cli fetchassumeutxo $srcFile $hash $destOk
if ($code -ne 0) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    throw "fetchassumeutxo OK path failed: $LastCliErr $LastCliOut"
}
Write-Host $LastCliOut
if ($LastCliOut -notmatch 'verified') {
    Write-Host "WARN: output missing verified field (still check file)"
}
$okPath = Join-Path $Data "regtest\$destOk"
# dogecoin datadir layout: regtest is network subdir under -datadir
# Actually GetDataDir() for regtest is datadir/regtest — relative dest is under that
if (-not (Test-Path $okPath)) {
    # try without regtest subdir
    $okPath2 = Join-Path $Data $destOk
    if (Test-Path $okPath2) { $okPath = $okPath2 }
    else {
        Get-ChildItem -Recurse $Data -Filter 'ok.dat' -EA SilentlyContinue | ForEach-Object { $okPath = $_.FullName }
    }
}
if (-not (Test-Path $okPath)) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    throw "OK dest not found under datadir"
}
$gotBytes = [System.IO.File]::ReadAllBytes($okPath)
$gotHash = ($sha.ComputeHash($gotBytes) | ForEach-Object { $_.ToString('x2') }) -join ''
if ($gotHash -ne $hash) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    throw "dest hash mismatch got=$gotHash want=$hash"
}
Write-Host "  OK path hash match: $okPath"

# Cleanup daemon
Invoke-Cli stop 2>$null | Out-Null
Start-Sleep -Seconds 2
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -EA SilentlyContinue }

Write-Host "=== FETCHASSUMEUTXO_SMOKE_OK ==="
exit 0
