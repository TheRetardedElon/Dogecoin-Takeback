# Phasing out Qt (Core Pro GUI strategy)

**Goal:** Ship a light, cross-platform control plane without Qt/MOC, while **dogecoind** remains the only consensus + wallet database process.

**Network / consensus:** Unchanged. Peers still speak Dogecoin P2P. Wallet keys stay in the node.

## Architecture (end state — also the 1.14.104 ship)

```
┌──────────────────────────┐         localhost JSON-RPC          ┌─────────────────────────┐
│ dogecoind (headless)     │◄───────────────────────────────────►│ dogecoin-pro-gui (ImGui) │
│ AppInitMain, chainstate, │   cookie or user/pass               │ splash → panels → arcade │
│ wallet, Fast Sync, P2P   │                                     │ no consensus on UI thread│
└──────────────────────────┘                                     └─────────────────────────┘
```

Optional: OS service / GPENode starts `dogecoind`; Pro GUI is always Path A.

## Shipping vs in-tree Qt

| Surface | 1.14.104 |
|---------|----------|
| Desktop | `dogecoin-pro-gui` (ImGui + GLFW + OpenGL3) |
| Hybrid picker | `corepro-launch.exe` (WIN32, no console) |
| Operator | `gpenode-tui` |
| `dogecoin-qt` | **Not packed** in the Windows setup or `.deb` |

Qt remains in `src/qt` as a fallback / unfinished native HTTP Meme Stream surface. Phase-out means replace features in order, not delete `src/qt` in one commit.

## Why Qt is not deleted tomorrow

| Still useful in-tree | Why |
|----------------------|-----|
| Native Meme Stream HTTP (feed / publish / like / upload) | ImGui ships WebView of `memestream.gopastearth.com` + RPC tip; no native HTTP client yet |
| Locale / coin-control polish | Not all wallet niceties have ImGui counterparts |
| Historical Fast Sync / theme code | Reference while ImGui panels finish |

## Phases

### Phase 0 — Foundation (done)
- `pro-gui/` ImGui docking shell + gold assets
- Idle sleep + V-Sync
- Path A boot splash (wait for RPC / offline)
- JSON-RPC client: chain, network, wallet balance, peers, console, send/list
- Arcade → GPE hub URL
- Node startup I/O: non-blocking Discover / externalip / DNS seed source

### Phase 1 — Operator-complete shell (done)
- [x] Live Home / Network / Chain panels
- [x] Send / History / Console via RPC
- [x] Options dialog tabs (Main / Wallet / Network / Window / Display / Theme / Connection / Hybrid)
- [x] Console help catalog + Help menu + command delivery
- [x] Settings persist (`pro-gui-settings.ini`) + node conf export
- [x] Windows smoke `.exe` (`release/smoke-pro-gui/`, static MinGW)
- [x] Receive: getnewaddress + clipboard / dogecoin: URI + QR
- [x] Mining panel (`getmininginfo`)
- [x] Fast Sync panel: listassumeutxo, fetchassumeutxomanifest, loadtxoutset, activatesnapshot
- [x] QR image for receive / invoices / POS Charge (`pro-gui/src/qr_tiny.*`)
- [x] First-run datadir Intro replacement (ImGui Welcome + cloud-path refuse)
- [x] Installer: Client / Server / Hybrid (one dogecoind) — see `doc/install-roles.md`
- [x] Hybrid ask / Remember (`hybrid-ui.txt`) via native `corepro-launch` (no PowerShell picker)

### Phase 2 — Product parity for daily use (in progress)
- [x] Fast Sync: RPC wrappers driven from ImGui
- [x] First-run: datadir picker + Prefer Fast Sync SoftSets (Intro replacement)
- [x] Business Center: dashboard, invoices (persist + auto-watch), POS keypad / Charge / payment QR
- [x] Meme Stream: Home rail (WebView) + Stream (full site) + Submit (publish ≤ 69 KiB + tip)
- [x] Home Meme Stream rail (compact cards + Wow / Tip)
- [x] Meme Stream image attach on publish (≤ 69 KiB multipart) in ImGui
- [ ] Multi-wallet if node supports it

### Phase 3 — Default GUI = ImGui (done for 1.14.104)
- [x] Installer launches `dogecoind` (Client: GUI/`dogeinit`; Server/Hybrid: service) + `dogecoin-pro-gui`
- [x] `dogecoin-qt` not packed (optional / deprecated)
- [x] Product docs + GitHub README point at Path A

### Phase 4 — Remove Qt from default builds
- [ ] `configure --without-gui` becomes default product path
- [ ] Qt sources remain optional for legacy

## Close / tray / Exit (product contract)

| Action | GUI | Node |
|--------|-----|------|
| Window **X** / Hide / Minimize to tray | One tray icon (the Desktop GUI) | Stays up |
| File → **Exit** / tray **Quit and stop node** | Process exits after splash | Stop Windows service `DogecoinGPENode` if present, then RPC `stop`, wait for flush |
| Hybrid tray while hidden | Same icon: Show Desktop GUI or Open Operator TUI | Unchanged |

Never force-kill `dogecoind` during IBD or flush.

## What we will not do

- Run `AppInitMain` on the ImGui render thread
- Weaken validation or change wire protocol for “speed”
- Drop LevelDB without a migration/flag plan (LevelDB stays the default live engine; MDBX is opt-in)
- Require other nodes to understand our local DB format

## Relationship to performance work

Making **dogecoind** start and sync faster (dbcache, Fast Sync, async DNS, later storage engine) shortens:

1. Qt splash (if still used from source)
2. ImGui Path A splash (RPC wait)
3. Time-to-usable-wallet

Splash does not need cosmetic “speed hacks.”

## Commands

```bash
# Node (example)
dogecoind -daemon

# Control plane (WSL cross-build used on this machine)
wsl -e bash /mnt/c/dogedev/pro-gui/scripts/build-wsl.sh
wsl -e bash -c 'cd /mnt/c/dogedev/pro-gui/build && ./dogecoin-pro-gui'
# Windows: cmake --build pro-gui/build-win --target dogecoin-pro-gui
# or: .\pro-gui\scripts\run-pro-gui.ps1
```

## Key docs

- [client-startup-splash-breakdown.md](client-startup-splash-breakdown.md)
- [startup-performance.md](startup-performance.md)
- [pro-gui-imgui.md](pro-gui-imgui.md)
- [pro-gui/README.md](../pro-gui/README.md)
- [install-roles.md](install-roles.md)
