# Sync C:\dogedev (source of truth) -> C:\dogedevGPEnode (GPE site node tree)
# - Fast-forward git to dogedev/master when possible
# - Copy uncommitted source/docs/scripts from dogedev
# - Preserve GPE-only paths (do not delete)
$ErrorActionPreference = "Stop"
$Src = "C:\dogedev"
$Dst = "C:\dogedevGPEnode"

if (-not (Test-Path $Src)) { throw "missing $Src" }
if (-not (Test-Path $Dst)) { throw "missing $Dst" }

Write-Host "=== 1) Git fast-forward dogedevGPEnode to dogedev/master ==="
Push-Location $Dst
try {
    $remotes = git remote
    if ($remotes -notcontains "dogedev") {
        git remote add dogedev $Src
        Write-Host "added remote dogedev -> $Src"
    } else {
        git remote set-url dogedev $Src
    }
    git fetch dogedev
    $before = (git rev-parse --short HEAD).Trim()
    # Prefer clean FF; if dirty tracked files, stash only tracked
    $status = git status --porcelain
    $trackedDirty = $status | Where-Object { $_ -match '^[ MADRCU]' -or $_ -match '^M' }
    if ($trackedDirty) {
        Write-Host "stashing local tracked changes on GPEnode..."
        git stash push -m "pre-sync-from-dogedev $(Get-Date -Format o)" --keep-index 2>$null
        git stash push -u -m "pre-sync-from-dogedev-untracked-skip" -- path/that/does/not/exist 2>$null
        git stash push -m "pre-sync-from-dogedev $(Get-Date -Format o)" -- .
    }
    git merge --ff-only dogedev/master
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FF failed; attempting merge dogedev/master..."
        git merge dogedev/master -m "Merge dogedev master into dogedevGPEnode (GPE site node)"
        if ($LASTEXITCODE -ne 0) { throw "git merge failed" }
    }
    $after = (git rev-parse --short HEAD).Trim()
    Write-Host "HEAD $before -> $after"
} finally {
    Pop-Location
}

Write-Host "=== 2) Copy uncommitted / WIP trees from dogedev (preserve GPE-only) ==="
# Robocopy: /E recurse, /XO only newer optional - we want dogedev to win for shared paths
# /XD exclude dirs that are GPE-only or build junk
$excludeDirs = @(
    ".git",
    "smoke-run",
    "release",
    "local",
    "deploy",
    "html\gpeintegration",
    "html\dochub",
    "src\.deps",
    "src\qt\.deps",
    "src\qt\test",
    "src\test\.deps",
    "src\leveldb",
    "src\secp256k1",
    "src\univalue",
    "autom4te.cache",
    "depends\work",
    "depends\built",
    "depends\sources",
    "depends\SDKs",
    "depends\x86_64-w64-mingw32",
    "depends\x86_64-unknown-linux-gnu",
    "node_modules",
    "__pycache__"
)

function Copy-Tree {
    param($Rel)
    $s = Join-Path $Src $Rel
    $d = Join-Path $Dst $Rel
    if (-not (Test-Path $s)) { return }
    New-Item -ItemType Directory -Force -Path $d | Out-Null
    $xd = $excludeDirs | ForEach-Object { $_ }
    # robocopy exit codes 0-7 are success
    & robocopy $s $d /E /IS /IT /NFL /NDL /NJH /NJS /nc /ns /np `
        /XD .git smoke-run release local deploy gpeintegration dochub .deps work built sources SDKs x86_64-w64-mingw32 node_modules __pycache__ autom4te.cache `
        /XF *.o *.a *.exe *.qm *.Po *.Tpo *.lo *.la *.pyc
    $code = $LASTEXITCODE
    if ($code -ge 8) { throw "robocopy failed $Rel code=$code" }
    Write-Host "  synced $Rel (robocopy=$code)"
}

# Core product trees
@(
    "src\node",
    "src\qt",
    "src\rpc",
    "src\test",
    "doc",
    "html\docs",
    "scripts",
    "qa",
    "depends\packages",
    "share"
) | ForEach-Object { Copy-Tree $_ }

# Top-level docs / makefile includes that matter
$files = @(
    "README.md",
    "DOGECOIN_CHANGELOG.md",
    "DOGECOIN_ARCHITECTURE_MERMAID.md",
    "src\Makefile.am",
    "src\Makefile.qt.include",
    "src\Makefile.test.include",
    "src\init.cpp",
    "src\validation.cpp",
    "src\validation.h",
    "src\chainparams.cpp",
    "src\chainparams.h",
    "src\ibdstats.cpp",
    "src\ibdstats.h",
    "src\net_processing.cpp",
    "configure.ac"
)
foreach ($f in $files) {
    $sf = Join-Path $Src $f
    $df = Join-Path $Dst $f
    if (Test-Path $sf) {
        $dir = Split-Path $df -Parent
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
        Copy-Item -Force $sf $df
        Write-Host "  file $f"
    }
}

Write-Host "=== 3) Verify key capabilities present on GPEnode ==="
$checks = @(
    "src\node\utxo_snapshot.cpp",
    "src\node\snapshot_fetch.cpp",
    "src\node\chainstate.cpp",
    "src\qt\win_image_decode.cpp",
    "src\qt\memestreamclient.cpp",
    "doc\tiered-storage-and-fast-sync.md",
    "doc\assumeutxo-dogecoin-1.14.md",
    "doc\memestream-gpe-handoff.md",
    "scripts\smoke-fetchassumeutxo-e2e.ps1"
)
$fail = 0
foreach ($c in $checks) {
    $p = Join-Path $Dst $c
    if (Test-Path $p) { Write-Host "  OK $c" }
    else { Write-Host "  MISSING $c"; $fail++ }
}

# GPE-only still present?
$gpeOnly = @(
    "html\gpeintegration",
    "scripts\remote_bootstrap.py",
    "scripts\node_status.py"
)
Write-Host "=== 4) GPE-only assets (should still exist) ==="
foreach ($c in $gpeOnly) {
    $p = Join-Path $Dst $c
    if (Test-Path $p) { Write-Host "  OK keep $c" }
    else { Write-Host "  WARN missing GPE-only $c (may never have been committed)" }
}

Write-Host "=== 5) Write SYNC stamp ==="
$stamp = @"
SYNC dogedev -> dogedevGPEnode
UTC: $(Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
dogedev HEAD: $((git -C $Src rev-parse HEAD).Trim())
GPEnode HEAD: $((git -C $Dst rev-parse HEAD).Trim())
Direction: dogedev is source of truth for Core Pro capabilities.
GPE site ops (scripts/*, html/gpeintegration, deploy, local) preserved.
"@
Set-Content -Path (Join-Path $Dst "SYNC_FROM_DOGEDEV.txt") -Value $stamp -Encoding UTF8

if ($fail -gt 0) { throw "sync incomplete, $fail missing checks" }
Write-Host "SYNC_DOGEDEV_TO_GPENODE_OK"
exit 0
