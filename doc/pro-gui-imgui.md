# Core Pro ImGui control plane (`pro-gui`)

**Audience:** developers and operators.  
**Shipping GUI (1.14.104):** `dogecoin-pro-gui` (Dear ImGui + GLFW + OpenGL3).  
**Not packed:** `dogecoin-qt`. Sources stay in `src/qt` as a fallback.

## Motivation

Qt 5 is heavy for a pure control/ops surface (event loop, widget tree, large dependency surface). The product goals:

1. **Light host footprint** — especially on low-RAM / integrated-GPU machines.
2. **Process isolation** — UI crash or redraw cost must not take down consensus.
3. **Dockable operator panels** — IBD, peers, logs, arcade, business.
4. **Arcade / Meme Stream without embedding Chromium for chrome** — WebView of GPE hubs; tips and invoices stay on RPC.
5. **Themes** without a CSS/DOM engine (ImGui style tables + gold PNG assets).

## Architecture

```
dogecoind (--daemon / service)     dogecoin-pro-gui
  validation, wallet, P2P    <-->    ImGui frame loop
  Fast Sync, RPC server              WaitEventsTimeout + V-Sync
                                     WebView2 for arcade / meme feed
```

**Trust model:** node holds keys and validates. GUI is a client of localhost RPC. Do not expose RPC to the LAN for “pretty UI.”

Windows Hybrid Start Menu uses `corepro-launch.exe` (no console). It is not a PowerShell/WinForms picker.

### Idle / potato rules

| Rule | Implementation in `pro-gui/src/main.cpp` |
|------|------------------------------------------|
| Sleep when idle | `glfwWaitEventsTimeout(1.0)` |
| Cap FPS | `glfwSwapInterval(1)` |
| Don’t steal from node | Separate process; node owns CPU for crypto/db |

### Close / tray / Exit

| Action | GUI | Node |
|--------|-----|------|
| **X** / Hide / Minimize to tray | One tray icon (this GUI). Node stays up. |
| File → Exit / tray **Quit and stop node** | Shutdown splash (waits up to 15 min) | RPC stop + flush, **then** stop `DogecoinGPENode` so it cannot restart the node |
| Process up, RPC dead | Restart `DogecoinGPENode` once | Same one dogecoind |

## Product panels

| Panel | How it works |
|-------|----------------|
| Home / Network / Chain / Mining | Live RPC snapshots |
| Send / Receive | `sendtoaddress` / `getnewaddress`; QR of `dogecoin:` URI |
| History / Console | `listtransactions` + `help` catalog |
| Fast Sync | Manifest fetch, SHA-256, `loadtxoutset` / activate |
| Arcade | WebView2 → `https://arcade.gopastearth.com/` |
| Meme Stream | Home rail = GPE WebView. Tab = Stream (full site) + Submit (native HTTP publish ≤ 69 KiB + tip). |
| Business | Local invoices in `business-invoices.txt`; `getnewaddress` + `getreceivedbyaddress` watch; POS keypad / Charge / QR |

QR encoder: `pro-gui/src/qr_tiny.*` (byte mode, ECC M, versions 1–10). Enough for Dogecoin payment URIs. Not a full QR library.

Native HTTP is `pro-gui/src/memestream_http.*` (WinINet on Windows). Publish with an optional image is multipart field `image` (max 69 KiB JPEG/PNG/GIF/WebP) on the same POST as title/body/wallet. The built-in publish key is the same obfuscated material as Qt (`src/qt/memestreampublishkey.cpp`).

## Arcade

- **Not** a local catalog of Flappy/Blaster icons.
- Panel embeds **GPE hub**: `https://arcade.gopastearth.com/` (alt: `gopastearth.com/arcade`).
- Games and categories are defined by the GPE site.

## Assets

| Step | Location |
|------|----------|
| Source (chroma green JPG) | User `Downloads/imgui-ideas` |
| Convert script | `scripts/chroma-key-pro-gui-assets.py` |
| Output | `pro-gui/assets/**/*.png` + `catalog.json` |
| Runtime copy | `pro-gui/build-win/assets/` (CMake POST_BUILD) |

## Build & smoke

See **[pro-gui/README.md](../pro-gui/README.md)** for full commands.

```bash
wsl -e bash /mnt/c/dogedev/pro-gui/scripts/build-wsl.sh
wsl -e bash /mnt/c/dogedev/pro-gui/scripts/smoke-pro-gui.sh
```

Windows install copy lives next to `dogecoind` under `C:\Program Files\Dogecoin\`.

## Relationship to Qt Core Pro

| Feature | ImGui 1.14.104 (shipped) | Qt (in-tree only) |
|---------|--------------------------|-------------------|
| Wallet send/receive + QR | Live RPC + `qr_tiny` | Full |
| Fast Sync | ImGui panel + RPC | Dialog still present |
| Arcade | WebView2 hub | Qt cabinet still in source |
| Meme Stream | Full GPE site (WebView) + Submit (≤ 69 KiB image) | Native HTTP leftover |
| Business / POS | Dashboard, invoices, keypad, QR | Original widgets |
| Installer | NSIS + `.deb` pack ImGui, not Qt | Not packed |

## Related docs

- [qt-phase-out.md](qt-phase-out.md) — phase checklist
- [install-roles.md](install-roles.md) — Client / Server / Hybrid
- [tiered-storage-and-fast-sync.md](tiered-storage-and-fast-sync.md) — Fast Sync product model
- [memestream-gpe-handoff.md](memestream-gpe-handoff.md) — GPE HTTP contract (native client still pending)
- Root [README.md](../README.md) — shipping Core Pro overview
