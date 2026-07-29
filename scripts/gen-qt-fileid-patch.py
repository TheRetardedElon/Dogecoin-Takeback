#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
import shutil
import os

src_tarball = Path.home() / "dogedev-winbuild/depends/sources/qtbase-opensource-src-5.7.1.tar.gz"
out_patch = Path("/mnt/c/dogedev/depends/patches/qt/mingw_file_id_info.patch")

tmpdir = Path(tempfile.mkdtemp(prefix="qtfix-"))
qtbase = tmpdir / "qtbase"
qtbase.mkdir()
subprocess.check_call(
    ["tar", "--strip-components=1", "-xf", str(src_tarball), "-C", str(qtbase)]
)
f = qtbase / "src/corelib/io/qfilesystemengine_win.cpp"
text = f.read_text(encoding="utf-8", errors="replace")
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
new = (
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
if old not in text:
    raise SystemExit("pattern not found in qfilesystemengine_win.cpp")
f.write_text(text.replace(old, new, 1), encoding="utf-8")

# Create unified diff from original tree layout: a/qtbase/...
# Work from tmpdir parent with qtbase as only content
proc = subprocess.run(
    ["diff", "-u", str(f) + ".orig", str(f)],
    capture_output=True,
    text=True,
)
# We didn't keep orig; regenerate via extract of original section using second file
# Simpler: write patch manually with correct context
patch = """--- a/qtbase/src/corelib/io/qfilesystemengine_win.cpp
+++ b/qtbase/src/corelib/io/qfilesystemengine_win.cpp
@@ -627,13 +627,13 @@
 // MinGW-64 defines FILE_ID_128 as of gcc-4.8.1 along with FILE_SUPPORTS_INTEGRITY_STREAMS
 #    if !(defined(Q_CC_MINGW) && defined(FILE_SUPPORTS_INTEGRITY_STREAMS))
 typedef struct _FILE_ID_128 {
     BYTE  Identifier[16];
 } FILE_ID_128, *PFILE_ID_128;
-#    endif // !(Q_CC_MINGW && FILE_SUPPORTS_INTEGRITY_STREAMS)
 
 typedef struct _FILE_ID_INFO {
     ULONGLONG VolumeSerialNumber;
     FILE_ID_128 FileId;
 } FILE_ID_INFO, *PFILE_ID_INFO;
+#    endif // !(Q_CC_MINGW && FILE_SUPPORTS_INTEGRITY_STREAMS)
 #  endif // if defined (Q_CC_MINGW) || (defined(Q_CC_MSVC) && (_MSC_VER < 1700 || WINVER <= 0x0601))
 
 // File ID for Windows up to version 7.
"""
out_patch.write_text(patch, encoding="utf-8")

# verify apply
# restore original from tarball again for dry-run
f_orig = text  # original text before replace
f.write_text(f_orig, encoding="utf-8")
r = subprocess.run(
    ["patch", "-p1", "--dry-run", "-i", str(out_patch)],
    cwd=str(tmpdir),
    capture_output=True,
    text=True,
)
print(r.stdout)
print(r.stderr)
if r.returncode != 0:
    raise SystemExit(f"patch dry-run failed: {r.returncode}")
print("PATCH_OK", out_patch)
shutil.rmtree(tmpdir, ignore_errors=True)
