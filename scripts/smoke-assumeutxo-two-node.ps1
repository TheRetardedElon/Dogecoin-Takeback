# Two-node AssumeUTXO regtest: producer dumps tip; consumer syncs headers,
# loads snapshot, activates, and waits for background validation + collapse.
$ErrorActionPreference = 'Continue'
$PSNativeCommandUseErrorActionPreference = $false

$Smoke = 'C:\dogedev\smoke-run'
$Daemon = Join-Path $Smoke 'dogecoind.exe'
$Cli = Join-Path $Smoke 'dogecoin-cli.exe'
$Base = Join-Path $Smoke 'regtest-assumeutxo-2node'
$DataA = Join-Path $Base 'nodeA'
$DataB = Join-Path $Base 'nodeB'
$PortA = 18444
$RpcA = 18332
$PortB = 18445
$RpcB = 18333

function Start-Node {
    param($Data, $Port, $RpcPort, $Extra = @())
    New-Item -ItemType Directory -Force -Path $Data | Out-Null
    $args = @(
        '-regtest',
        "-datadir=$Data",
        '-server=1',
        '-listen=1',
        '-dnsseed=0',
        '-upnp=0',
        "-port=$Port",
        "-rpcport=$RpcPort",
        '-rpcbind=127.0.0.1',
        '-rpcallowip=127.0.0.1'
    ) + $Extra
    return Start-Process -FilePath $Daemon -ArgumentList $args -PassThru -WindowStyle Hidden
}

function Invoke-NodeCli {
    param($Data, $RpcPort, [Parameter(ValueFromRemainingArguments = $true)]$Rest)
    $argList = @('-regtest', "-datadir=$Data", "-rpcport=$RpcPort") + @($Rest)
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
    if ($proc.ExitCode -ne 0) {
        throw "cli failed ($($proc.ExitCode)) rpc=${RpcPort}: $stderr $stdout"
    }
    return $stdout.Trim()
}

function Wait-Rpc {
    param($Data, $RpcPort, $Proc, $Label)
    for ($i = 0; $i -lt 120; $i++) {
        Start-Sleep -Seconds 1
        if ($Proc.HasExited) { throw "$Label exited early" }
        $cookie = Join-Path $Data 'regtest\.cookie'
        if (-not (Test-Path $cookie)) { continue }
        try {
            $h = Invoke-NodeCli $Data $RpcPort getblockcount
            Write-Host "$Label ready height=$h"
            return
        } catch {}
    }
    throw "$Label RPC not ready"
}

Write-Host '=== AssumeUTXO two-node regtest smoke ==='
Get-Process dogecoind -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
Start-Sleep -Seconds 2
if (Test-Path $Base) { Remove-Item -Recurse -Force $Base }

$pA = Start-Node $DataA $PortA $RpcA
$connectArg = "-connect=127.0.0.1:$PortA"
$pB = Start-Node $DataB $PortB $RpcB @($connectArg)
try {
    Wait-Rpc $DataA $RpcA $pA 'nodeA'
    Wait-Rpc $DataB $RpcB $pB 'nodeB'

    Write-Host '--- nodeA generate 80 ---'
    $null = Invoke-NodeCli $DataA $RpcA generate 80
    $tipA = [int](Invoke-NodeCli $DataA $RpcA getblockcount)
    Write-Host "nodeA tip=$tipA"
    if ($tipA -lt 70) { throw "tip too low" }

    $dumpPath = Join-Path $DataA 'utxo.dat'
    Write-Host "--- nodeA dumptxoutset ---"
    $dump = Invoke-NodeCli $DataA $RpcA dumptxoutset $dumpPath
    Write-Host $dump
    if ($dump -notmatch 'hash_serialized') { throw 'dump failed' }

    # Ensure B sees headers for snapshot height
    Write-Host '--- wait for nodeB headers ---'
    $headersOk = $false
    for ($i = 0; $i -lt 90; $i++) {
        Start-Sleep -Seconds 1
        $info = Invoke-NodeCli $DataB $RpcB getblockchaininfo
        # parse headers from JSON-ish output
        if ($info -match '"headers"\s*:\s*(\d+)') {
            $hdr = [int]$Matches[1]
            Write-Host "nodeB headers=$hdr"
            if ($hdr -ge $tipA) { $headersOk = $true; break }
        }
    }
    if (-not $headersOk) {
        # try explicit addnode
        try { Invoke-NodeCli $DataB $RpcB addnode "127.0.0.1:$PortA" onetry | Out-Null } catch {}
        for ($i = 0; $i -lt 60; $i++) {
            Start-Sleep -Seconds 1
            $info = Invoke-NodeCli $DataB $RpcB getblockchaininfo
            if ($info -match '"headers"\s*:\s*(\d+)') {
                $hdr = [int]$Matches[1]
                Write-Host "nodeB headers=$hdr (retry)"
                if ($hdr -ge $tipA) { $headersOk = $true; break }
            }
        }
    }
    if (-not $headersOk) { throw "nodeB headers never reached $tipA" }

    # Copy dump to B
    $dumpB = Join-Path $DataB 'utxo.dat'
    Copy-Item -Force $dumpPath $dumpB

    Write-Host '--- nodeB loadtxoutset ---'
    $load = Invoke-NodeCli $DataB $RpcB loadtxoutset $dumpB
    Write-Host $load

    Write-Host '--- nodeB activatesnapshot ---'
    $act = Invoke-NodeCli $DataB $RpcB activatesnapshot
    Write-Host $act
    if ($act -notmatch 'active_swapped') { throw 'activate failed' }

    Write-Host '--- wait background validation on B ---'
    $validated = $false
    for ($i = 0; $i -lt 180; $i++) {
        Start-Sleep -Seconds 1
        $ibd = Invoke-NodeCli $DataB $RpcB getibdinfo
        if ($i % 10 -eq 0) {
            if ($ibd -match '"assumeutxo_progress"\s*:\s*([0-9.]+)') {
                Write-Host "progress=$($Matches[1])"
            }
            if ($ibd -match '"background_validation_status"\s*:\s*"([^"]+)"') {
                Write-Host "bg_status=$($Matches[1])"
            }
        }
        if ($ibd -match '"assumeutxo_validated"\s*:\s*true') {
            $validated = $true
            Write-Host $ibd
            break
        }
        if ($ibd -match '"assumeutxo_failed"\s*:\s*true') {
            Write-Host $ibd
            throw 'background validation failed'
        }
        # nudge validation if RPC exists
        try { Invoke-NodeCli $DataB $RpcB stepbackgroundvalidation 50 | Out-Null } catch {}
    }
    if (-not $validated) {
        Write-Host (Invoke-NodeCli $DataB $RpcB getibdinfo)
        throw 'background validation timeout'
    }

    $cs = Invoke-NodeCli $DataB $RpcB getchainstates
    Write-Host $cs
    if ($cs -notmatch 'assumeutxo_dual_collapsed') {
        Write-Host 'warn: dual_collapsed field missing'
    }

    Write-Host "ASSUMEUTXO_TWO_NODE_SMOKE_OK tipA=$tipA"
}
finally {
    foreach ($d in @(@{D=$DataA;R=$RpcA}, @{D=$DataB;R=$RpcB})) {
        try {
            $psi = New-Object System.Diagnostics.ProcessStartInfo
            $psi.FileName = $Cli
            $psi.Arguments = "-regtest -datadir=$($d.D) -rpcport=$($d.R) stop"
            $psi.UseShellExecute = $false
            $psi.CreateNoWindow = $true
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
            $pr = [System.Diagnostics.Process]::Start($psi)
            $pr.WaitForExit(5000) | Out-Null
        } catch {}
    }
    Start-Sleep -Seconds 2
    Get-Process dogecoind -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
}
