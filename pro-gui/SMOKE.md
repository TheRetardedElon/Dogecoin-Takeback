# Smoke log — dogecoin-pro-gui

## 2026-08-12 (WSL + WSLg)

| Item | Value |
|------|--------|
| Host | Windows + WSL2 |
| Binary | `pro-gui/build/dogecoin-pro-gui` (~2.0 MB) |
| Display | `DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-0` |
| Command | `bash pro-gui/scripts/smoke-pro-gui.sh` |
| Result | **SMOKE_OK** |

### Checks

1. Binary executable and `build/assets/catalog.json` present  
2. Process alive after 1 second  
3. Process alive after ~8 seconds  
4. Log contains `pro-gui assets: assets`  
5. No `GLFW error` / `FATAL` / segfault lines  

### Interactive

```bash
cd pro-gui/build && ./dogecoin-pro-gui
```

Or from Windows:

```powershell
.\pro-gui\scripts\run-pro-gui.ps1
```

You should see the gold-dark shell, sidebar icons, and Arcade panel that can open the GPE hub.
