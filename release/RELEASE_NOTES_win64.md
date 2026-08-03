# Dogecoin Core Pro / Takeback — Windows x64

## Version
- Client: **1.14.100** (Pro / Takeback tree)
- Target: Windows 64-bit (x86_64)
- Built: WSL2 cross-compile (depends + MinGW-w64 posix, Qt 5.7.1)
- Date: 2026-08-02

## Artifacts
- `dogecoin-1.14.100-win64.zip` — portable package
- `dogecoin-1.14.100-win64-setup.exe` — NSIS installer
- `SHA256SUMS-win64.txt` — checksums

## What's new in this build
- **Arcade tab** — client-only retro mini-games shelf
- **Retr-Doge Shibe Blaster** — pure Qt shooter (QPainter + QTimer)
  - Title art: your Retr-Doge artwork
  - Controls: Left/Right (or A/D) move, SPACE shoot, P pause, R restart
  - Blast HATERS and FUD blobs for MUCH SCORE
  - High score saved locally (no wallet/network)

## Nav order
Home → Meme Stream → Business → Network → Options → **Arcade**

## Install
### Installer
1. Run `dogecoin-1.14.100-win64-setup.exe`
2. Launch Dogecoin Core from the Start Menu

### Portable
1. Extract the zip
2. Run `dogecoin-qt.exe`

## Notes
- First sync downloads the full Dogecoin chain (or enable pruned mode in Options).
- Windows SmartScreen may warn on unsigned builds — expected until code-signed.
- Pure DOGE client (no EVM). Arcade does not touch wallet or settlement node.

## Verify
```
sha256sum -c SHA256SUMS-win64.txt
```
