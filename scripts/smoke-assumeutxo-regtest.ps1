# AssumeUTXO regtest smoke (Windows PE binaries in smoke-run)
# dump -> load -> activate on a short mined chain
$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false
$Smoke = 'C:\dogedev\smoke-run'
$Data = 'C:\dogedev\smoke-run\regtest-assumeutxo-data'
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
    if ($proc.ExitCode -ne 0) {
        throw "cli failed ($($proc.ExitCode)): $stderr $stdout args=$($psi.Arguments)"
    }
    return $stdout.Trim()
}

Write-Host "=== AssumeUTXO regtest smoke ==="
if (-not (Test-Path $Daemon)) { throw "missing $Daemon" }
if (-not (Test-Path $Cli)) { throw "missing $Cli" }

Get-Process dogecoind -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
Start-Sleep -Seconds 2
if (Test-Path $Data) { Remove-Item -Recurse -Force $Data }
New-Item -ItemType Directory -Path $Data | Out-Null

Write-Host "Starting dogecoind -regtest..."
Write-Host "datadir=$Data"
$p = Start-Process -FilePath $Daemon -ArgumentList @(
    '-regtest',
    $DatadirArg,
    '-server=1',
    '-listen=0',
    '-dnsseed=0',
    '-upnp=0'
) -PassThru -WindowStyle Hidden

$ready = $false
for ($i = 0; $i -lt 120; $i++) {
    Start-Sleep -Seconds 1
    if ($p.HasExited) {
        $log = Join-Path $Data 'regtest\debug.log'
        if (Test-Path $log) { Get-Content $log -Tail 40 }
        throw "dogecoind exited early code=$($p.ExitCode)"
    }
    $cookie = Join-Path $Data 'regtest\.cookie'
    if (-not (Test-Path $cookie)) { continue }
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Cli
    $psi.Arguments = "-regtest $DatadirArg getblockcount"
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    if ($proc.ExitCode -eq 0) {
        $ready = $true
        Write-Host "getblockcount=$($stdout.Trim())"
        break
    }
}
if (-not $ready) {
    $log = Join-Path $Data "regtest\debug.log"
    if (Test-Path $log) { Get-Content $log -Tail 40 }
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    throw "dogecoind not ready"
}
Write-Host "RPC ready (pid=$($p.Id))"

try {
    Write-Host "--- getchainstates ---"
    $cs = Invoke-Cli getchainstates | Out-String
    Write-Host $cs
    if ($cs -notmatch "chainstates") { throw "getchainstates missing chainstates" }

    Write-Host "--- listassumeutxo ---"
    $list = Invoke-Cli listassumeutxo | Out-String
    Write-Host $list

    Write-Host "--- generate 70 ---"
    $gen = Invoke-Cli generate 70
    Write-Host "generated $($gen.Split(',').Count) block hash lines (ok if JSON array)"
    $height = [int](Invoke-Cli getblockcount)
    Write-Host "tip height=$height"
    if ($height -lt 60) { throw "height too low: $height" }

    $dumpPath = Join-Path $Data 'utxo.dat'
    Write-Host "--- dumptxoutset $dumpPath ---"
    $dumpJson = Invoke-Cli dumptxoutset $dumpPath
    Write-Host $dumpJson
    if (-not (Test-Path $dumpPath)) { throw "dump file missing" }
    if ($dumpJson -notmatch 'hash_serialized') { throw "dump missing hash_serialized" }

    Write-Host "--- loadtxoutset (no activate) ---"
    $load = Invoke-Cli loadtxoutset $dumpPath
    Write-Host $load
    if ($load -notmatch 'base_height') { throw "load failed" }

    Write-Host "--- getchainstates after load ---"
    Write-Host (Invoke-Cli getchainstates)

    Write-Host "--- activatesnapshot ---"
    $act = Invoke-Cli activatesnapshot
    Write-Host $act
    if ($act -notmatch 'active_swapped') { throw "activate missing active_swapped" }

    Write-Host "--- getibdinfo ---"
    $ibd = Invoke-Cli getibdinfo
    Write-Host $ibd
    if ($ibd -notmatch 'snapshot_active') { throw "getibdinfo missing snapshot_active" }

    Write-Host "--- getchainstates after activate ---"
    Write-Host (Invoke-Cli getchainstates)

    Write-Host "ASSUMEUTXO_REGTEST_SMOKE_OK height=$height"
}
finally {
    Write-Host "Stopping dogecoind..."
    try { Invoke-Cli stop 2>$null | Out-Null } catch {}
    Start-Sleep -Seconds 2
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -EA SilentlyContinue }
}
