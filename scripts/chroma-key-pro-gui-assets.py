#!/usr/bin/env python3
"""Chroma-key pure green-screen JPGs from imgui-ideas into RGBA PNGs for dogecoin-pro-gui.

Default key: bright green (near #00FF00). Soft edge via distance threshold.
Skips optional per-game icons when --skip-games (flappy/blaster).
"""
from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

from PIL import Image
import numpy as np

# Optional: not required when Arcade loads arcade.gopastearth.com
SKIP_GAME_ICONS = {
    "flappyStyleIcon.jpg",
    "blasterIcon.jpg",
}

# Sidebar icons we always want at rail size variants
SIDEBAR_MAX = 64
ICON_MAX = 128
SPLASH_MAX = 512
BORDER_MAX = 256


def chroma_to_rgba(
    img: Image.Image,
    key_rgb=(0, 255, 0),
    hard=45.0,
    soft=75.0,
) -> Image.Image:
    """Key out green. hard: fully transparent below this distance. soft: fade band."""
    rgba = img.convert("RGBA")
    arr = np.asarray(rgba).astype(np.float32)
    rgb = arr[:, :, :3]
    kr, kg, kb = key_rgb
    dist = np.sqrt(
        (rgb[:, :, 0] - kr) ** 2
        + (rgb[:, :, 1] - kg) ** 2
        + (rgb[:, :, 2] - kb) ** 2
    )
    # Also treat pure-ish greens (high G, low R/B) as key — generative green screen
    r, g, b = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    greenish = (g > 180) & (g > r * 1.35) & (g > b * 1.35) & (r < 120) & (b < 120)
    alpha = np.ones(dist.shape, dtype=np.float32) * 255.0
    alpha[dist <= hard] = 0.0
    mid = (dist > hard) & (dist < soft)
    alpha[mid] = ((dist[mid] - hard) / (soft - hard)) * 255.0
    alpha[greenish & (dist < soft * 1.2)] = np.minimum(
        alpha[greenish & (dist < soft * 1.2)],
        np.clip((dist[greenish & (dist < soft * 1.2)] - hard * 0.5) / soft * 255.0, 0, 255),
    )
    out = arr.copy()
    out[:, :, 3] = alpha
    # Zero RGB where fully transparent to avoid green fringe bleed
    mask0 = alpha < 1.0
    out[mask0, 0] = 0
    out[mask0, 1] = 0
    out[mask0, 2] = 0
    return Image.fromarray(out.astype(np.uint8), "RGBA")


def fit_max(img: Image.Image, max_side: int) -> Image.Image:
    w, h = img.size
    m = max(w, h)
    if m <= max_side:
        return img
    scale = max_side / float(m)
    nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
    return img.resize((nw, nh), Image.Resampling.LANCZOS)


def trim_transparent(img: Image.Image, pad: int = 2) -> Image.Image:
    if img.mode != "RGBA":
        return img
    alpha = img.split()[-1]
    bbox = alpha.getbbox()
    if not bbox:
        return img
    l, t, r, b = bbox
    l = max(0, l - pad)
    t = max(0, t - pad)
    r = min(img.width, r + pad)
    b = min(img.height, b + pad)
    return img.crop((l, t, r, b))


def max_for(rel: str) -> int:
    name = Path(rel).name.lower()
    parent = str(Path(rel).parent).replace("\\", "/").lower()
    if "splash" in name or "shibainu" in name:
        return SPLASH_MAX
    if "border" in parent or "border" in name or name.startswith("hor") or name.startswith("ver"):
        return BORDER_MAX
    if "sidebar" in parent:
        return SIDEBAR_MAX
    if name in ("doged_metallicgold.jpg", "smcirculargoldcoin.jpg"):
        return ICON_MAX
    return ICON_MAX


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--src",
        type=Path,
        default=Path(r"C:\Users\Jeramiah\Downloads\imgui-ideas"),
    )
    ap.add_argument(
        "--dst",
        type=Path,
        default=Path(r"C:\dogedev\pro-gui\assets"),
    )
    ap.add_argument("--skip-games", action="store_true", default=True)
    ap.add_argument("--keep-games", action="store_true", help="Also export flappy/blaster")
    ap.add_argument("--hard", type=float, default=50.0)
    ap.add_argument("--soft", type=float, default=90.0)
    args = ap.parse_args()
    skip_games = args.skip_games and not args.keep_games

    src: Path = args.src
    dst: Path = args.dst
    if not src.is_dir():
        print(f"FATAL: source not found: {src}")
        return 1

    # Wipe only generated trees we own
    for sub in ("icons", "sidebar", "borders", "brand", "arcade", "chrome", "status"):
        p = dst / sub
        if p.exists():
            shutil.rmtree(p)
        p.mkdir(parents=True, exist_ok=True)

    manifest = []
    count = 0
    skipped = []

    for path in sorted(src.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix.lower() not in (".jpg", ".jpeg", ".png"):
            continue
        rel = path.relative_to(src).as_posix()
        name = path.name
        if skip_games and name in SKIP_GAME_ICONS:
            skipped.append(rel)
            continue

        img = Image.open(path)
        keyed = chroma_to_rgba(img, hard=args.hard, soft=args.soft)
        keyed = trim_transparent(keyed)
        keyed = fit_max(keyed, max_for(rel))

        # Bucket by role
        low = rel.lower()
        if "sidebar" in low:
            bucket = "sidebar"
        elif "border" in low or name.startswith("hor") or name.startswith("ver") or "Corner" in name or "thinGold" in name or name == "ZEmgu.jpg":
            bucket = "borders"
        elif name in ("shibaInuonDSplash.jpg", "dogeD_metallicGold.jpg", "smCircularGoldcoin.jpg"):
            bucket = "brand"
        elif name.startswith("arcadecontrol") or name == "insertCoin.jpg":
            bucket = "arcade"
        elif name in ("closeIcon.jpg", "minimizeIcon.jpg", "dockpinIcon.jpg"):
            bucket = "chrome"
        elif name in ("errorIcon.jpg", "successIcon.jpg", "statusGlyph.jpg"):
            bucket = "status"
        else:
            bucket = "icons"

        out_name = path.stem + ".png"
        out_path = dst / bucket / out_name
        out_path.parent.mkdir(parents=True, exist_ok=True)
        keyed.save(out_path, "PNG", optimize=True)

        entry = {
            "src": rel,
            "dst": f"{bucket}/{out_name}",
            "w": keyed.width,
            "h": keyed.height,
            "bytes": out_path.stat().st_size,
        }
        manifest.append(entry)
        count += 1
        print(f"OK  {rel} -> {bucket}/{out_name} ({keyed.width}x{keyed.height})")

    man_path = dst / "manifest.json"
    man_path.write_text(json.dumps({"count": count, "skipped": skipped, "assets": manifest}, indent=2), encoding="utf-8")

    # Logical map for the shell (arcade = hub URL, not game icons)
    catalog = {
        "arcade_hub_url": "https://gopastearth.com/arcade",
        "arcade_hub_alt": "https://arcade.gopastearth.com/",
        "sidebar": [
            {"id": "home", "file": "sidebar/homeSidebarIcon.png", "label": "Home"},
            {"id": "send", "file": "sidebar/sendSidebarIcon.png", "label": "Send"},
            {"id": "tx", "file": "sidebar/txSidebarIcon.png", "label": "History"},
            {"id": "network", "file": "sidebar/networkSidebarIcon.png", "label": "Network"},
            {"id": "blocks", "file": "sidebar/blocksSidebarIcon.png", "label": "Chain"},
            {"id": "arcade", "file": "sidebar/dogeArcadeSidebarIcon.png", "label": "Arcade"},
            {"id": "meme", "file": "sidebar/memeStreamSidebarIcon.png", "label": "Meme Stream"},
            {"id": "business", "file": "sidebar/busCenterSidebarIcon.png", "label": "Business"},
            {"id": "console", "file": "sidebar/consoleSidebarIcon.png", "label": "Console"},
            {"id": "mining", "file": "sidebar/miningSidebarIcon.png", "label": "Mining"},
            {"id": "settings", "file": "sidebar/settingsSidebarIcon.png", "label": "Settings"},
        ],
        "brand": {
            "splash": "brand/shibaInuonDSplash.png",
            "mark": "brand/dogeD_metallicGold.png",
            "coin": "brand/smCircularGoldcoin.png",
        },
        "actions": {
            "send": "icons/sendIcon.png",
            "receive": "icons/receiveIcon.png",
            "balance": "icons/walletballanceIcon.png",
            "fastsync": "icons/fastsyncRocketIcon.png",
            "peers_map": "icons/peersMapIcon.png",
            "theme": "icons/themeSwitcherIcon.png",
            "lock": "icons/lockEncrypticon.png",
            "qr": "icons/qrStyleIcon.png",
            "copy": "icons/copyClipboardIcon.png",
        },
        "note": "Arcade content loads from GPE hub URL; no per-game flappy/blaster icons required.",
    }
    (dst / "catalog.json").write_text(json.dumps(catalog, indent=2), encoding="utf-8")

    print(f"\nDone: {count} PNGs -> {dst}")
    if skipped:
        print("Skipped (hub-hosted games):", ", ".join(skipped))
    print(f"Manifest: {man_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
