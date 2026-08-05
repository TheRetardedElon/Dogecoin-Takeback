# Dogecoin Core Pro / Takeback — Windows x64

## Version
- Client: **1.14.101** (release)
- Target: Windows 64-bit (x86_64)
- Built: WSL2 cross-compile (depends + MinGW-w64 posix, Qt 5.7.1)
- Date: 2026-08-05

## Artifacts
- `dogecoin-1.14.101-win64.zip` — portable package
- `dogecoin-1.14.101-win64-setup.exe` — NSIS installer
- `SHA256SUMS-win64.txt` — checksums

## What's new in 1.14.101

### IBD P0 — performance & operability (non-consensus)
- **`getibdinfo` RPC** — stall disconnects, rescue fetches, download timeouts, chainstate flush cost
- **IBD rescue fetch** — when preferred download peers stall/saturate, other peers can fetch blocks
- **`-ibdrescue`** — default **on**; `-ibdrescue=0` restores preferred-only download
- **`-debug=ibd`** — detailed IBD / flush / stall logs
- **Network page** — live “IBD telemetry: stall · rescue · dl-to · flush”
- **`getpeerinfo`** fields: `preferred_download`, `stalling`, `stalling_seconds`, `blocks_in_flight`

Design notes: `doc/ibd-p0-peer-telemetry.md`

### Arcade (client only; from 1.14.100 line)
- **Arcade** left-nav tab
- **Retr-Doge Shibe Blaster** — pure Qt retro shooter

### Safety
- Pure DOGE client — **no consensus / AuxPoW / PoW changes**

## Nav
Home → Send/Receive/Transactions → Network → Doge Business → Meme Stream → Arcade → Console

## Install
### Installer
1. Run `dogecoin-1.14.101-win64-setup.exe`
2. Launch Dogecoin Core from the Start Menu

### Portable
1. Extract the zip
2. Run `dogecoin-qt.exe`

## Ops (IBD)
```
dogecoin-cli getibdinfo
dogecoin-cli getpeerinfo
dogecoind -debug=ibd
dogecoind -ibdrescue=0
```

## Notes
- First sync downloads the full chain (or use prune mode in Options).
- Windows SmartScreen may warn on unsigned builds.

## Verify
```
sha256sum -c SHA256SUMS-win64.txt
```
