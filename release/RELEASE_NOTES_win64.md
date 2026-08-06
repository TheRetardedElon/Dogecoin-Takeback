# Dogecoin Core Pro 1.14.101 — Windows x64

## Artifacts
- `dogecoin-1.14.101-win64.zip` — portable
- `dogecoin-1.14.101-win64-setup.exe` — NSIS installer
- `SHA256SUMS-win64.txt`

## What's inside
- **dogecoin-qt.exe** — Core Pro GUI (Home, Business, Meme Stream, Network, Arcade, themes)
- **daemon/dogecoind.exe** — full node
- **bin/dogecoin-cli.exe** / **daemon/dogecoin-cli.exe**
- Optional **bin/dogecoin-tx.exe**

## Features (Pro / Takeback)
- Pure DOGE settlement (no EVM wrap product path)
- IBD telemetry (`getibdinfo`), stall rescue, parallel download, ASMAP
- AssumeUTXO dual chainstate RPCs (regtest proven; mainnet map empty until attested)
- Doge Business invoices/POS, Meme Stream tips on-chain, Arcade mini-game

## Build
- Cross-compiled via WSL2 + MinGW + depends (Qt 5.7.1)
- Strip applied to release copies

## Smoke
```powershell
powershell -ExecutionPolicy Bypass -File scripts\smoke-assumeutxo-regtest.ps1
powershell -ExecutionPolicy Bypass -File scripts\smoke-assumeutxo-two-node.ps1
```

## Install
Run `dogecoin-1.14.101-win64-setup.exe` or unzip portable and launch `dogecoin-qt.exe`.

**Pre-release** — review before high-value use.
