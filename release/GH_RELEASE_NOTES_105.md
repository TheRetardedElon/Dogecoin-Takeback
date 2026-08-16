# Dogecoin Core Pro 1.14.105

ImGui Path A — one `dogecoind`. Qt is not packed. Installer asks **Client / Server / Hybrid**.

Open source: https://github.com/TheRetardedElon/Dogecoin-Takeback

## Windows

| File | What |
|------|------|
| `dogecoin-1.14.105-win64-setup-rpcsecure.exe` | NSIS setup (recommended) |
| `dogecoin-1.14.105-win64.zip` | Portable tree (GUI + daemon + assets) |
| `SHA256SUMS-1.14.105.txt` | Digests |

## Linux

| File | What |
|------|------|
| `dogecoin-core-pro_1.14.105-1_amd64.deb` | Debian / Ubuntu |
| `dogecoin-gpenode_1.14.105-1_all.deb` | Transitional — pulls Core Pro |

```
sudo apt update && sudo apt install dogecoin-core-pro
```

Testnet: `dogecoin-core-pro --testnet` or the **Dogecoin Core Pro Testnet** menu entry.

## This release

Windows setup/zip rebuilt **2026-08-16** (GUI only — `dogecoind` is still the 1.14.105 daemon):

- Home / Chain never paint height `-1` on a busy RPC; last good tip is held
- UTXO cache bar (`dbcache` fill). Near-full warns that a flush can pause IBD for minutes
- Tray / minimized: probe every 12s; restore kicks an immediate refresh
- Options → Performance: `-par` slider (0 = auto). Next node start
- Linux `.deb` files are unchanged

- **Testnet launch:** Start Menu + desktop **Dogecoin Core Pro Testnet** (green coin icon) → `corepro-launch.exe --testnet`
- Same flag on `dogecoin-pro-gui --ui gfx --testnet`, `gpenode-tui --testnet`, `dogecoin-cli -testnet`
- ImGui uses Matrix (green) on testnet; operator TUI is green with a TESTNET header
- Testnet does **not** start or stop the mainnet Windows service. Stop mainnet first (one `dogecoind`). RPC 44555, datadir `testnet3`
- Fast Sync CDN remains mainnet-only

Still true from 1.14.104:

- One tray icon. X / hide = tray, node stays. File → Exit / tray Quit = RPC stop, wait for flush, then stop `DogecoinGPENode`
- Home Meme Stream rail; Meme tab = Stream (full site) + Submit (≤ 69 KiB)
- Optional Tor: drop Expert Bundle at `Dogecoin\tor\tor.exe`. Options → Privacy (off by default)
- RPC: `.cookie` first, then `dogecoin.conf`. GUI ini does not store the password

## Verify

```
sha256sum -c SHA256SUMS-1.14.105.txt
```
