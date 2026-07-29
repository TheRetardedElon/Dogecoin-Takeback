#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
import shutil

src = Path.home() / "dogedev-winbuild/depends/sources/qtbase-opensource-src-5.7.1.tar.gz"
out = Path("/mnt/c/dogedev/depends/patches/qt/mingw_file_id_info.patch")
out2 = Path.home() / "dogedev-winbuild/depends/patches/qt/mingw_file_id_info.patch"

td = Path(tempfile.mkdtemp())
orig = td / "orig" / "qtbase"
new = td / "new" / "qtbase"
orig.mkdir(parents=True)
new.mkdir(parents=True)
subprocess.check_call(["tar", "--strip-components=1", "-xf", str(src), "-C", str(orig)])
subprocess.check_call(["tar", "--strip-components=1", "-xf", str(src), "-C", str(new)])

rel = Path("src/corelib/io/qfilesystemengine_win.cpp")
t = (new / rel).read_text(encoding="utf-8", errors="replace")
old = (
    "#    if !(defined(Q_CC_MINGW) && defined(FILE_SUPPORTS_INTEGRITY_STREAMS))\n"
    "typedef struct _FILE_ID_128 {\n"
    "    BYTE  Identifier[16];\n"
    "} FILE_ID_128, *PFILE_ID_128;\n"
    "#    endif // !(Q_CC_MINGW && FILE_SUPPORTS_INTEGRITY_STREAMS)\n"
    "\n"
    "typedef struct _FILE_ID_INFO {\n"
    "    ULONGLONG VolumeSerialNumber;\n"
    "    FILE_ID_128 FileId;\n"
    "} FILE_ID_INFO, *PFILE_ID_INFO;\n"
    "#  endif // if defined (Q_CC_MINGW) || (defined(Q_CC_MSVC) && (_MSC_VER < 1700 || WINVER <= 0x0601))\n"
)
new_txt = (
    "#    if !(defined(Q_CC_MINGW) && defined(FILE_SUPPORTS_INTEGRITY_STREAMS))\n"
    "typedef struct _FILE_ID_128 {\n"
    "    BYTE  Identifier[16];\n"
    "} FILE_ID_128, *PFILE_ID_128;\n"
    "\n"
    "typedef struct _FILE_ID_INFO {\n"
    "    ULONGLONG VolumeSerialNumber;\n"
    "    FILE_ID_128 FileId;\n"
    "} FILE_ID_INFO, *PFILE_ID_INFO;\n"
    "#    endif // !(Q_CC_MINGW && FILE_SUPPORTS_INTEGRITY_STREAMS)\n"
    "#  endif // if defined (Q_CC_MINGW) || (defined(Q_CC_MSVC) && (_MSC_VER < 1700 || WINVER <= 0x0601))\n"
)
if old not in t:
    raise SystemExit("pattern not found")
(new / rel).write_text(t.replace(old, new_txt, 1), encoding="utf-8")

proc = subprocess.run(
    [
        "diff",
        "-u",
        f"a/qtbase/{rel.as_posix()}",
        f"b/qtbase/{rel.as_posix()}",
    ],
    cwd=str(td),
    capture_output=True,
    text=True,
)
# diff needs files named under a/ and b/
# redo with proper paths:
afile = td / "a" / "qtbase" / rel
bfile = td / "b" / "qtbase" / rel
afile.parent.mkdir(parents=True)
bfile.parent.mkdir(parents=True)
shutil.copy2(orig / rel, afile)
shutil.copy2(new / rel, bfile)
proc = subprocess.run(
    ["diff", "-u", str(afile.relative_to(td)), str(bfile.relative_to(td))],
    cwd=str(td),
    capture_output=True,
    text=True,
)
# diff returns 1 when files differ
if proc.returncode not in (0, 1):
    raise SystemExit(proc.stderr)
patch = proc.stdout
# normalize headers
lines = patch.splitlines(True)
if lines:
    lines[0] = f"--- a/qtbase/{rel.as_posix()}\n"
if len(lines) > 1:
    lines[1] = f"+++ b/qtbase/{rel.as_posix()}\n"
text = "".join(lines)
out.parent.mkdir(parents=True, exist_ok=True)
out2.parent.mkdir(parents=True, exist_ok=True)
out.write_text(text, encoding="utf-8", newline="\n")
out2.write_text(text, encoding="utf-8", newline="\n")

# dry-run
r = subprocess.run(
    ["patch", "-p1", "--dry-run", "-i", str(out)],
    cwd=str(td / "orig_tree") if False else str(td),
    capture_output=True,
    text=True,
)
# apply against a clean tree at td/check
check = td / "check"
check.mkdir()
subprocess.check_call(["tar", "--strip-components=0", "-xf", str(src), "-C", str(check)])
# tarball extracts to qtbase-opensource-src-5.7.1 or flat? --strip 0
# use orig dir structure with qtbase root
r = subprocess.run(
    ["patch", "-p1", "--dry-run", "-i", str(out)],
    cwd=str(td / "orig").parent if False else str(td),
    capture_output=True,
    text=True,
)
# Create tree: check2/qtbase from orig
check2 = td / "check2"
(check2 / "qtbase").parent.mkdir(parents=True, exist_ok=True)
if (check2 / "qtbase").exists():
    shutil.rmtree(check2 / "qtbase")
shutil.copytree(orig, check2 / "qtbase")
r = subprocess.run(
    ["patch", "-p1", "--dry-run", "-i", str(out)],
    cwd=str(check2),
    capture_output=True,
    text=True,
)
print(r.stdout)
print(r.stderr)
print(text)
if r.returncode != 0:
    raise SystemExit(f"dry-run failed {r.returncode}")
print("PATCH_OK")
shutil.rmtree(td, ignore_errors=True)
