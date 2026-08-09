# Product P1 e2e: mine -> dumptxoutset -> sha256 -> fetchassumeutxo -> loadtxoutset activate
# Uses smoke-run PE with fetchassumeutxo linked.
$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false
$Smoke = 'C:\dogedev\smoke-run'
$Data = 'C:\dogedev\smoke-run\regtest-fetch-e2e-data'
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
    if ($proc.ExitCode -ne 0) {
        throw "cli failed ($($proc.ExitCode)): $stderr $stdout"
    }
    return $stdout.Trim()
}

Write-Host "=== fetchassumeutxo E2E smoke (dump -> fetch -> load/activate) ==="
if (-not (Test-Path $Daemon)) { throw "missing $Daemon" }
if (-not (Test-Path $Cli)) { throw "missing $Cli" }

Get-Process dogecoind -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
Start-Sleep -Seconds 2
if (Test-Path $Data) { Remove-Item -Recurse -Force $Data }
New-Item -ItemType Directory -Path $Data | Out-Null

$p = Start-Process -FilePath $Daemon -ArgumentList @(
    '-regtest', $DatadirArg, '-server=1', '-listen=0', '-dnsseed=0', '-upnp=0'
) -PassThru -WindowStyle Hidden

$ready = $false
for ($i = 0; $i -lt 90; $i++) {
    Start-Sleep -Seconds 1
    if ($p.HasExited) { throw "dogecoind exited early" }
    try {
        $null = Invoke-Cli getblockcount
        $ready = $true
        break
    } catch { }
}
if (-not $ready) { throw "dogecoind not ready" }
Write-Host "RPC ready"

try {
    $null = Invoke-Cli help fetchassumeutxo
    Write-Host "--- generate 70 ---"
    $null = Invoke-Cli generate 70
    $height = [int](Invoke-Cli getblockcount)
    Write-Host "tip height=$height"
    if ($height -lt 60) { throw "height too low" }

    # Dump under datadir (absolute path for Windows)
    $dumpPath = Join-Path $Data 'regtest\utxo_dump.dat'
    $dumpDir = Split-Path $dumpPath -Parent
    if (-not (Test-Path $dumpDir)) { New-Item -ItemType Directory -Path $dumpDir | Out-Null }
    Write-Host "--- dumptxoutset ---"
    $dumpJson = Invoke-Cli dumptxoutset $dumpPath
    Write-Host $dumpJson
    if (-not (Test-Path $dumpPath)) { throw "dump missing" }

    $sha = [System.Security.Cryptography.SHA256]::Create()
    $fileBytes = [System.IO.File]::ReadAllBytes($dumpPath)
    $hash = ($sha.ComputeHash($fileBytes) | ForEach-Object { $_.ToString('x2') }) -join ''
    Write-Host "artifact sha256=$hash size=$($fileBytes.Length)"

    # Stage a "CDN local" copy outside datadir to fetch from
    $cdnCopy = Join-Path $Data 'cdn-source\utxo.dat'
    New-Item -ItemType Directory -Path (Split-Path $cdnCopy) -Force | Out-Null
    Copy-Item $dumpPath $cdnCopy -Force

    Write-Host "--- fetchassumeutxo (stream-hash from local CDN path) ---"
    $fetch = Invoke-Cli fetchassumeutxo $cdnCopy $hash 'snapshots/from_cdn.dat'
    Write-Host $fetch
    if ($fetch -notmatch '"verified": true') { throw "fetch not verified" }

    Write-Host "--- loadtxoutset (no activate) ---"
    $load = Invoke-Cli loadtxoutset snapshots/from_cdn.dat
    Write-Host $load
    if ($load -notmatch 'coins_loaded') { throw "load unexpected: $load" }

    Write-Host "--- activatesnapshot ---"
    $act = Invoke-Cli activatesnapshot
    Write-Host $act
    if ($act -notmatch 'active_swapped') { throw "activate unexpected: $act" }

    Write-Host "--- getibdinfo ---"
    $ibd = Invoke-Cli getibdinfo
    Write-Host $ibd

    Write-Host "--- getchainstates ---"
    Write-Host (Invoke-Cli getchainstates)

    Write-Host "FETCHASSUMEUTXO_E2E_SMOKE_OK height=$height"
}
finally {
    try { Invoke-Cli stop 2>$null | Out-Null } catch {}
    Start-Sleep -Seconds 2
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -EA SilentlyContinue }
}
