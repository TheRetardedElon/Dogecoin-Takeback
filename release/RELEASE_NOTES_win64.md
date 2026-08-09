# Dogecoin Core Pro 1.14.102 Windows x64 — FULL RELEASE

Fresh MinGW cross-build of dogecoind, dogecoin-cli, dogecoin-tx, dogecoin-qt
after Pro source sync. **Not** a smoke-run package.

## Highlights vs 1.14.101

- **Fast Sync / CDN product path (P1 partial)**
  - Stream-and-hash snapshot fetch (`fetchassumeutxo`)
  - GPE-compatible **`latest.json`** resolve (`fetchassumeutxomanifest`)
  - Official default: `https://sync.doge.gopastearth.com/latest.json`
  - Options: Prefer Fast Sync, custom URL + artifact SHA-256 (SoftSet to CLI)
  - Intro Fast path SoftSets prune (~5.5 GB target)
- **AssumeUTXO engineering** (dump/load/activate dual chainstate) remains
- **Meme Stream** load/SSL/image decode improvements
- Still **compatible with the public Dogecoin network** (same consensus)

## Artifacts

- `dogecoin-1.14.102-win64-setup.exe`
- `dogecoin-1.14.102-win64.zip`
- `SHA256SUMS-win64.txt`

## Notes

- Custom URL/SHA empty → uses official manifest when published; until a `.dat`
  is hosted, Fast Sync falls back to normal P2P + prune.
- Mainnet `mapAssumeutxo` attestations fill after the first community dump.
- GPE settlement node can run a matching Linux daemon for `dumptxoutset`.

Built: (filled by package script)
