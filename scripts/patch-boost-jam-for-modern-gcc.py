#!/usr/bin/env python3
"""Patch Boost 1.63 jam engine sources so b2 builds on modern host GCC (11+/14+/15)."""
from __future__ import annotations

import sys
from pathlib import Path


def patch_tree(root: Path) -> None:
    sh = root / "tools/build/src/engine/build.sh"
    jam = root / "tools/build/src/engine/build.jam"
    if not sh.is_file() or not jam.is_file():
        raise SystemExit(f"boost engine files missing under {root}")

    sh_text = sh.read_text(encoding="utf-8", errors="replace")
    sh_text2 = sh_text.replace(
        'BOOST_JAM_CC="gcc -DNT"',
        'BOOST_JAM_CC="gcc-11 -std=gnu89 -DNT"',
    )
    sh_text2 = sh_text2.replace(
        "        BOOST_JAM_CC=gcc\n",
        '        BOOST_JAM_CC="gcc-11 -std=gnu89"\n',
    )
    # Idempotent if already patched
    if sh_text2 == sh_text and "gcc-11" not in sh_text:
        raise SystemExit("build.sh: expected gcc lines not found")
    sh.write_text(sh_text2, encoding="utf-8")

    jam_text = jam.read_text(encoding="utf-8", errors="replace")
    old = (
        'toolset gcc gcc : "-o " : -D\n'
        "    : -pedantic -fno-strict-aliasing"
    )
    new = (
        'toolset gcc gcc-11 : "-o " : -D\n'
        "    : -std=gnu89 -fno-strict-aliasing -Wno-implicit-function-declaration"
    )
    if old in jam_text:
        jam.write_text(jam_text.replace(old, new, 1), encoding="utf-8")
    elif "gcc-11" in jam_text and "Wno-implicit-function-declaration" in jam_text:
        pass  # already patched
    else:
        raise SystemExit("build.jam: expected gcc toolset block not found")

    print(f"patched: {root}")


def write_depends_patch(out: Path, root: Path) -> None:
    """Generate a unified diff patch against original tree (root must be unpatched copy)."""
    # Prefer regenerating via applying transforms to bak if present
    pass


def main() -> None:
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <boost-source-root>", file=sys.stderr)
        raise SystemExit(2)
    patch_tree(Path(sys.argv[1]).resolve())


if __name__ == "__main__":
    main()
