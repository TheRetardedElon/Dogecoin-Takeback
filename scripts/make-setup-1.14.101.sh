#!/usr/bin/env bash
# Build a correct NSIS installer for 1.14.101 from winbuild release/ binaries.
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
VERSION="${VERSION:-1.14.102}"
REL=dogecoin-${VERSION}-win64

test -f "$BUILD/release/dogecoin-qt.exe"
test -f "$BUILD/release/dogecoind.exe"
test -f "$BUILD/release/dogecoin-cli.exe"

NSI=$(mktemp /tmp/setup-XXXXXX.nsi)
cat > "$NSI" <<EOF
Name "Dogecoin Core (64-bit)"
RequestExecutionLevel highest
SetCompressor /SOLID lzma

!define REGKEY "SOFTWARE\$(^Name)"
!define VERSION ${VERSION}
!define COMPANY "Dogecoin Core project"
!define URL https://github.com/TheRetardedElon/Dogecoin-Takeback

!define MUI_ICON "${SRC}/share/pixmaps/dogecoin.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${SRC}/share/pixmaps/nsis-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${SRC}/share/pixmaps/nsis-header.bmp"
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_REGISTRY_KEY \${REGKEY}
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME StartMenuGroup
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Dogecoin Core"
!define MUI_FINISHPAGE_RUN \$INSTDIR\\dogecoin-qt.exe
!define MUI_UNICON "\${NSISDIR}\\Contrib\\Graphics\\Icons\\modern-uninstall.ico"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${SRC}/share/pixmaps/nsis-wizard.bmp"
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

!include Sections.nsh
!include MUI2.nsh
!include x64.nsh

Var StartMenuGroup

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application \$StartMenuGroup
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English

OutFile "${OUT}/${REL}-setup.exe"
InstallDir \$PROGRAMFILES64\\Dogecoin
CRCCheck on
XPStyle on
BrandingText " "
ShowInstDetails show
VIProductVersion \${VERSION}.0
VIAddVersionKey ProductName "Dogecoin Core"
VIAddVersionKey ProductVersion "\${VERSION}"
VIAddVersionKey CompanyName "\${COMPANY}"
VIAddVersionKey CompanyWebsite "\${URL}"
VIAddVersionKey FileVersion "\${VERSION}"
VIAddVersionKey FileDescription "Dogecoin Core Pro / Takeback"
VIAddVersionKey LegalCopyright "Copyright (c) 2009-2026 The Dogecoin Core developers"
InstallDirRegKey HKCU "\${REGKEY}" Path
ShowUninstDetails show

Function .onInit
  \${IfNot} \${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "This installer requires a 64-bit version of Windows."
    Abort
  \${EndIf}
  \${If} \${RunningX64}
    SetRegView 64
  \${EndIf}
FunctionEnd

Section -Main SEC0000
    SetOutPath \$INSTDIR
    SetOverwrite on
    File "${BUILD}/release/dogecoin-qt.exe"
    File /oname=COPYING.txt "${SRC}/COPYING"
    File /oname=readme.txt "${SRC}/doc/README_windows.txt"
    SetOutPath \$INSTDIR\\daemon
    File "${BUILD}/release/dogecoind.exe"
    File "${BUILD}/release/dogecoin-cli.exe"
    SetOutPath \$INSTDIR
    WriteRegStr HKCU "\${REGKEY}\\Components" Main 1
SectionEnd

Section -post SEC0001
    WriteRegStr HKCU "\${REGKEY}" Path \$INSTDIR
    SetOutPath \$INSTDIR
    WriteUninstaller \$INSTDIR\\uninstall.exe
    !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory \$SMPROGRAMS\\\$StartMenuGroup
    CreateShortcut "\$SMPROGRAMS\\\$StartMenuGroup\\\$(^Name).lnk" \$INSTDIR\\dogecoin-qt.exe
    CreateShortcut "\$SMPROGRAMS\\\$StartMenuGroup\\Uninstall \$(^Name).lnk" \$INSTDIR\\uninstall.exe
    !insertmacro MUI_STARTMENU_WRITE_END
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" DisplayName "\$(^Name)"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" DisplayVersion "\${VERSION}"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" Publisher "\${COMPANY}"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" URLInfoAbout "\${URL}"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" DisplayIcon \$INSTDIR\\uninstall.exe
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" UninstallString \$INSTDIR\\uninstall.exe
    WriteRegDWORD HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" NoModify 1
    WriteRegDWORD HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)" NoRepair 1
SectionEnd

Section Uninstall
    Delete /REBOOTOK \$INSTDIR\\dogecoin-qt.exe
    Delete /REBOOTOK \$INSTDIR\\COPYING.txt
    Delete /REBOOTOK \$INSTDIR\\readme.txt
    Delete /REBOOTOK \$INSTDIR\\uninstall.exe
    Delete /REBOOTOK \$INSTDIR\\daemon\\dogecoind.exe
    Delete /REBOOTOK \$INSTDIR\\daemon\\dogecoin-cli.exe
    RMDir /REBOOTOK \$INSTDIR\\daemon
    RMDir /REBOOTOK \$INSTDIR
    !insertmacro MUI_STARTMENU_GETFOLDER Application \$StartMenuGroup
    Delete /REBOOTOK "\$SMPROGRAMS\\\$StartMenuGroup\\\$(^Name).lnk"
    Delete /REBOOTOK "\$SMPROGRAMS\\\$StartMenuGroup\\Uninstall \$(^Name).lnk"
    RMDir /REBOOTOK \$SMPROGRAMS\\\$StartMenuGroup
    DeleteRegKey HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\\$(^Name)"
    DeleteRegKey HKCU "\${REGKEY}"
SectionEnd
EOF

# Fix accidental double-escaping from heredoc - write cleaner with python
python3 <<'PY'
import os
from pathlib import Path
BUILD = "/home/theretardedelon/dogedev-winbuild"
SRC = "/mnt/c/dogedev"
OUT = "/mnt/c/dogedev/release"
VERSION = os.environ.get("VERSION", "1.14.102")
REL = f"dogecoin-{VERSION}-win64"
nsi = f'''Name "Dogecoin Core (64-bit)"
RequestExecutionLevel highest
SetCompressor /SOLID lzma

!define REGKEY "SOFTWARE\\$(^Name)"
!define VERSION {VERSION}
!define COMPANY "Dogecoin Core project"
!define URL https://github.com/TheRetardedElon/Dogecoin-Takeback

!define MUI_ICON "{SRC}/share/pixmaps/dogecoin.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "{SRC}/share/pixmaps/nsis-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "{SRC}/share/pixmaps/nsis-header.bmp"
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_REGISTRY_KEY ${{REGKEY}}
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME StartMenuGroup
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Dogecoin Core"
!define MUI_FINISHPAGE_RUN $INSTDIR\\dogecoin-qt.exe
!define MUI_UNICON "${{NSISDIR}}\\Contrib\\Graphics\\Icons\\modern-uninstall.ico"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "{SRC}/share/pixmaps/nsis-wizard.bmp"
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

!include Sections.nsh
!include MUI2.nsh
!include x64.nsh

Var StartMenuGroup

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuGroup
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English

OutFile "{OUT}/{REL}-setup.exe"
InstallDir $PROGRAMFILES64\\Dogecoin
CRCCheck on
XPStyle on
BrandingText " "
ShowInstDetails show
VIProductVersion ${{VERSION}}.0
VIAddVersionKey ProductName "Dogecoin Core"
VIAddVersionKey ProductVersion "${{VERSION}}"
VIAddVersionKey CompanyName "${{COMPANY}}"
VIAddVersionKey CompanyWebsite "${{URL}}"
VIAddVersionKey FileVersion "${{VERSION}}"
VIAddVersionKey FileDescription "Dogecoin Core Pro / Takeback"
VIAddVersionKey LegalCopyright "Copyright (c) 2009-2026 The Dogecoin Core developers"
InstallDirRegKey HKCU "${{REGKEY}}" Path
ShowUninstDetails show

Function .onInit
  ${{IfNot}} ${{RunningX64}}
    MessageBox MB_OK|MB_ICONSTOP "This installer requires a 64-bit version of Windows."
    Abort
  ${{EndIf}}
  ${{If}} ${{RunningX64}}
    SetRegView 64
  ${{EndIf}}
FunctionEnd

Section -Main SEC0000
    SetOutPath $INSTDIR
    SetOverwrite on
    File "{BUILD}/release/dogecoin-qt.exe"
    File /oname=COPYING.txt "{SRC}/COPYING"
    File /oname=readme.txt "{SRC}/doc/README_windows.txt"
    SetOutPath $INSTDIR\\daemon
    File "{BUILD}/release/dogecoind.exe"
    File "{BUILD}/release/dogecoin-cli.exe"
    SetOutPath $INSTDIR
    WriteRegStr HKCU "${{REGKEY}}\\Components" Main 1
SectionEnd

Section -post SEC0001
    WriteRegStr HKCU "${{REGKEY}}" Path $INSTDIR
    SetOutPath $INSTDIR
    WriteUninstaller $INSTDIR\\uninstall.exe
    !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory $SMPROGRAMS\\$StartMenuGroup
    CreateShortcut "$SMPROGRAMS\\$StartMenuGroup\\$(^Name).lnk" $INSTDIR\\dogecoin-qt.exe
    CreateShortcut "$SMPROGRAMS\\$StartMenuGroup\\Uninstall $(^Name).lnk" $INSTDIR\\uninstall.exe
    !insertmacro MUI_STARTMENU_WRITE_END
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" DisplayName "$(^Name)"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" DisplayVersion "${{VERSION}}"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" Publisher "${{COMPANY}}"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" URLInfoAbout "${{URL}}"
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" DisplayIcon $INSTDIR\\uninstall.exe
    WriteRegStr HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" UninstallString $INSTDIR\\uninstall.exe
    WriteRegDWORD HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" NoModify 1
    WriteRegDWORD HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)" NoRepair 1
SectionEnd

Section Uninstall
    Delete /REBOOTOK $INSTDIR\\dogecoin-qt.exe
    Delete /REBOOTOK $INSTDIR\\COPYING.txt
    Delete /REBOOTOK $INSTDIR\\readme.txt
    Delete /REBOOTOK $INSTDIR\\uninstall.exe
    Delete /REBOOTOK $INSTDIR\\daemon\\dogecoind.exe
    Delete /REBOOTOK $INSTDIR\\daemon\\dogecoin-cli.exe
    RMDir /REBOOTOK $INSTDIR\\daemon
    RMDir /REBOOTOK $INSTDIR
    !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuGroup
    Delete /REBOOTOK "$SMPROGRAMS\\$StartMenuGroup\\$(^Name).lnk"
    Delete /REBOOTOK "$SMPROGRAMS\\$StartMenuGroup\\Uninstall $(^Name).lnk"
    RMDir /REBOOTOK $SMPROGRAMS\\$StartMenuGroup
    DeleteRegKey HKCU "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\$(^Name)"
    DeleteRegKey HKCU "${{REGKEY}}"
SectionEnd
'''
path = Path('/tmp/setup-1.14.101.nsi')
path.write_text(nsi)
print(path)
PY

makensis -V2 /tmp/setup-1.14.101.nsi
ls -lah "${OUT}/${REL}-setup.exe"
(
  cd "$OUT"
  sha256sum "${REL}.zip" "${REL}-setup.exe" | tee SHA256SUMS-win64.txt
)
echo SETUP_OK
