# Dogecoin Core Pro GUI (ImGui)

**Shipping desktop** for Core Pro 1.14.104. [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) over localhost JSON-RPC. `dogecoind` owns consensus and the wallet. This process never runs `AppInit` on the render thread.

`dogecoin-qt` is **not packed** in the installer. Qt remains in `src/qt` as a fallback.

## Process model

```
┌─────────────────────┐     127.0.0.1 JSON-RPC :22555     ┌──────────────────────────┐
│  dogecoind          │◄─────────────────────────────────►│  dogecoin-pro-gui        │
│  consensus, wallet  │                                   │  ImGui + OpenGL3 + GLFW  │
│  Fast Sync, P2P     │                                   │  sleeps when idle        │
└─────────────────────┘                                   └──────────────────────────┘
```

Windows Start Menu **Dogecoin Core Pro** runs `corepro-launch.exe` (WIN32, no console). On Hybrid it asks Desktop GUI vs Operator TUI unless Remember is set. **Dogecoin Core Pro Testnet** is the same launcher with `--testnet` (Matrix theme, RPC 44555, `testnet3` datadir; does not start the mainnet service).

| Goal | Approach |
|------|----------|
| Low-end / “potato” PCs | Tiny UI binary + GPU draw; no Qt object tree |
| Leave CPU/RAM for the node | **Separate process** from `dogecoind` |
| Idle must not burn cores | `glfwWaitEventsTimeout(1.0)` + V-Sync |
| Arcade / Meme Stream | WebView2 of GPE hubs; tips/sends via RPC |
| Themes | `ImGuiStyle` packs (Gold Dark / Matrix / Dim) |

## What is in this GUI

| Panel | Status |
|-------|--------|
| Boot / shutdown splash | Path A: wait for RPC; shutdown waits for flush |
| Home / Network / Chain / Mining | Live RPC |
| Send / Receive | `sendtoaddress` / `getnewaddress` + `dogecoin:` URI + QR |
| History / Console | `listtransactions` + help catalog |
| Fast Sync | Manifest / download / `loadtxoutset` / activate |
| Arcade | WebView2 of `arcade.gopastearth.com` |
| Meme Stream | Home rail = compact GPE WebView. Tab = **Stream** (full site) + **Submit** (≤ 69 KiB image + RPC tip). |
| Business | Dashboard, invoices (persist + auto-watch), POS keypad / Charge / QR |
| Options | Main (incl. network) / Wallet / Network / Privacy / Window / Display / Theme / Connection / Hybrid |
| Tray | **X** = hide (node stays). File → Exit / tray Quit = RPC stop then service stop |

## Layout

```
pro-gui/
  assets/              # RGBA PNGs + catalog.json / manifest.json
  src/                 # app, RPC, tray, WebView, QR, corepro-launch
  third_party/imgui/   # docking branch
  third_party/webview2/
  scripts/
    build-wsl.sh       # Linux / WSL native
    build-win-smoke.sh # MinGW cross to dogecoin-pro-gui.exe
    smoke-pro-gui.sh
  CMakeLists.txt
  README.md
```

## Close / tray / Exit

| Action | Result |
|--------|--------|
| Window **X** / Hide / Minimize to tray | GUI hides. `dogecoind` stays up. One tray icon (this process). Hybrid: Show Desktop GUI or Open Operator TUI. |
| File → **Exit** / tray **Quit and stop node** | RPC `stop`, wait for flush (up to 15 min), then stop `DogecoinGPENode` so SCM cannot restart it. Never force-kill. |

Never force-kill the node during IBD or flush.

## Build (Windows `.exe` — this machine)

Cross-compile from WSL (MinGW). Output: `pro-gui/build-win/dogecoin-pro-gui.exe` next to `assets/` and `WebView2Loader.dll`.

```bash
wsl -e bash -lc 'cd /mnt/c/dogedev/pro-gui/build-win && \
  /home/theretardedelon/tools/cmake/bin/cmake --build . --target dogecoin-pro-gui corepro-launch -j$(nproc)'
```

First-time configure (already done on this tree):

```bash
wsl -e bash /mnt/c/dogedev/pro-gui/scripts/build-win-smoke.sh
```

Copy into a live install (stop the GUI only — leave `dogecoind` running):

```powershell
Copy-Item C:\dogedev\pro-gui\build-win\dogecoin-pro-gui.exe "C:\Program Files\Dogecoin\" -Force
Copy-Item C:\dogedev\pro-gui\build-win\corepro-launch.exe "C:\Program Files\Dogecoin\" -Force
```

## Build (Linux / WSL native)

```bash
bash pro-gui/scripts/build-wsl.sh
```

If system packages are available:

```bash
sudo apt-get install -y cmake g++ libglfw3-dev libgl1-mesa-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
cd pro-gui
cmake -B build -DCMAKE_BUILD_TYPE=Release -DGLFW_BUILD_WAYLAND=OFF
cmake --build build -j
```

Without root on WSL, `scripts/fetch-x11-dev.sh` extracts X11 `.deb`s into `~/x11-prefix`.

## Smoke test

```
C:\dogedev\release\smoke-pro-gui\dogecoin-pro-gui-smoke.exe
```

Keep `assets\` next to the exe. Start `dogecoind` first (RPC 22555), then run the exe.

```bash
wsl -e bash /mnt/c/dogedev/pro-gui/scripts/smoke-pro-gui.sh
```

```powershell
.\pro-gui\scripts\run-pro-gui.ps1
.\pro-gui\scripts\run-pro-gui.ps1 -Smoke
```

**Expected:** `SMOKE_OK`, log line `pro-gui assets: assets`, no GLFW fatals.

## Assets

Source chroma JPGs: `%USERPROFILE%\Downloads\imgui-ideas` (green screen).

```bash
python scripts/chroma-key-pro-gui-assets.py
```

- Flappy / Blaster icons are **skipped** (arcade = hub URL).
- Buckets: `sidebar/`, `icons/`, `brand/`, `borders/`, `arcade/`, `chrome/`, `status/`.
- `assets/catalog.json` — nav map + hub URLs.

## Controls

| UI | Action |
|----|--------|
| Sidebar icons | Home, Send, History, Network, Chain, Arcade, Meme Stream, Business, Mining, Settings |
| Receive / Invoices / POS Charge | `dogecoin:` URI + QR (byte-mode ECC M, versions 1–10) |
| Arcade | WebView2 of the GPE arcade hub |
| Meme Stream | Native feed / publish / like / image attach + optional Site WebView + Tip |
| Business POS | Keypad, Clear, Charge, New sale, copy address/URI |
| Idle | Loop sleeps; wakes on input or ~1 Hz |

## Remaining (honest)

1. Multi-wallet if the node exposes it
2. Drop Qt from the default `configure` product path (`doc/qt-phase-out.md` Phase 4)

Startup speed for the *node* is in Core (`Discover` / `-externalip` / DNS seed source) — see [`doc/startup-performance.md`](../doc/startup-performance.md).

## License

Same as Dogecoin Core (MIT). ImGui is MIT (see `third_party/imgui/LICENSE.txt`).
