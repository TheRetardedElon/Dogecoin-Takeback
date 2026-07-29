# Dogecoin Core Pro / Takeback — Windows x64

## Version
- Client: **1.14.100** (Pro / Takeback tree)
- Target: Windows 64-bit (x86_64)
- Built: WSL2 cross-compile (depends + MinGW-w64 posix, Qt 5.7.1)

## Artifacts
- `dogecoin-1.14.100-win64.zip` — portable package
- `dogecoin-1.14.100-win64-setup.exe` — NSIS installer (if present)
- `SHA256SUMS-win64.txt` — checksums

## Zip layout
```
dogecoin-1.14.100-win64/
  dogecoin-qt.exe      # GUI wallet
  daemon/dogecoind.exe # full node daemon
  bin/dogecoin-cli.exe
  bin/dogecoin-tx.exe
  COPYING.txt
  readme.txt
```

## Install
### Installer
1. Run `dogecoin-1.14.100-win64-setup.exe`
2. Launch **Dogecoin Core** from the Start Menu

### Portable
1. Extract the zip
2. Run `dogecoin-qt.exe`

## Notes
- First sync downloads the full Dogecoin chain (or enable pruned mode in Options).
- Windows SmartScreen may warn on **unsigned** builds — expected until code-signed.
- Pure DOGE client (no EVM). Pro features: Home, Meme Stream, Business, Network, Options.

## Verify
```
sha256sum -c SHA256SUMS-win64.txt
```
