Name "Dogecoin Core (64-bit)"

RequestExecutionLevel highest
SetCompressor /SOLID lzma

# General Symbol Definitions
!define REGKEY "SOFTWARE\$(^Name)"
!define VERSION 1.14.103
!define COMPANY "Dogecoin Core project"
!define URL https://github.com/TheRetardedElon/Dogecoin-Takeback

# MUI Symbol Definitions
!define MUI_ICON "/mnt/c/dogedev/share/pixmaps/dogecoin.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "/mnt/c/dogedev/share/pixmaps/nsis-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "/mnt/c/dogedev/share/pixmaps/nsis-header.bmp"
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_REGISTRY_KEY ${REGKEY}
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME StartMenuGroup
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Dogecoin Core"
!define MUI_FINISHPAGE_RUN $INSTDIR\dogecoin-qt.exe
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "/mnt/c/dogedev/share/pixmaps/nsis-wizard.bmp"
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

# Included files
!include Sections.nsh
!include MUI2.nsh
!include LogicLib.nsh
!if "64" == "64"
!include x64.nsh
!endif

# Variables (must be before nsis-rpc-credentials.nsh so $RpcUser etc. resolve)
Var StartMenuGroup
Var RpcPassword
Var RpcUser
Var RpcAckCheckbox
Var RpcDlg
Var RpcPassEdit
Var DogecoinDataDir

# Unique RPC password page (shared with GPENode installer pattern)
!include "/mnt/c/dogedev/share/nsis-rpc-credentials.nsh"
ReserveFile "/mnt/c/dogedev/share/gen-rpc-password.ps1"

# Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom RpcCredentialsPage RpcCredentialsPageLeave
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuGroup
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

# Installer languages
!insertmacro MUI_LANGUAGE English

# Installer attributes
OutFile /mnt/c/dogedev/release/dogecoin-1.14.103-win64-setup-rpcsecure.exe
!if "64" == "64"
InstallDir $PROGRAMFILES64\Dogecoin
!else
InstallDir $PROGRAMFILES\Dogecoin
!endif
CRCCheck on
XPStyle on
BrandingText " "
ShowInstDetails show
VIProductVersion ${VERSION}.0
VIAddVersionKey ProductName "Dogecoin Core"
VIAddVersionKey ProductVersion "${VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey CompanyWebsite "${URL}"
VIAddVersionKey FileVersion "${VERSION}"
VIAddVersionKey FileDescription ""
VIAddVersionKey LegalCopyright ""
InstallDirRegKey HKCU "${REGKEY}" Path
ShowUninstDetails show

# Installer sections
Section -Main SEC0000
    SetOutPath $INSTDIR
    SetOverwrite on
    File /mnt/c/dogedev/release/dogecoin-qt.exe
    File /oname=COPYING.txt /mnt/c/dogedev/COPYING
    File /oname=readme.txt /mnt/c/dogedev/doc/README_windows.txt
    SetOutPath $INSTDIR\daemon
    File /mnt/c/dogedev/release/dogecoind.exe
    File /mnt/c/dogedev/release/dogecoin-cli.exe
    SetOutPath $INSTDIR\doc
    File /r /mnt/c/dogedev/doc\*.*
    SetOutPath $INSTDIR
    WriteRegStr HKCU "${REGKEY}\Components" Main 1
SectionEnd

Section -post SEC0001
    WriteRegStr HKCU "${REGKEY}" Path $INSTDIR
    SetOutPath $INSTDIR
    WriteUninstaller $INSTDIR\uninstall.exe
    ; Unique RPC credentials helper scripts
    File "/mnt/c/dogedev/share/write-install-conf.ps1"
    File "/mnt/c/dogedev/share/gen-rpc-password.ps1"
    ; Write dogecoin.conf into %APPDATA%\Dogecoin if missing (unique password per install)
    CreateDirectory "$DogecoinDataDir"
    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\write-install-conf.ps1" -DataDir "$DogecoinDataDir" -RpcUser "$RpcUser" -RpcPassword "$RpcPassword" -Profile core'
    Pop $1
    DetailPrint "write-install-conf exit=$1"
    !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory $SMPROGRAMS\$StartMenuGroup
    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\$(^Name).lnk" $INSTDIR\dogecoin-qt.exe
    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core (testnet, 64-bit).lnk" "$INSTDIR\dogecoin-qt.exe" "-testnet" "$INSTDIR\dogecoin-qt.exe" 1
    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\RPC credentials.lnk" notepad.exe "$DogecoinDataDir\RPC-CREDENTIALS.txt"
    CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Uninstall $(^Name).lnk" $INSTDIR\uninstall.exe
    !insertmacro MUI_STARTMENU_WRITE_END
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayName "$(^Name)"
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayVersion "${VERSION}"
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" Publisher "${COMPANY}"
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" URLInfoAbout "${URL}"
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" DisplayIcon $INSTDIR\uninstall.exe
    WriteRegStr HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" UninstallString $INSTDIR\uninstall.exe
    WriteRegDWORD HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" NoModify 1
    WriteRegDWORD HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" NoRepair 1
    WriteRegStr HKCR "dogecoin" "URL Protocol" ""
    WriteRegStr HKCR "dogecoin" "" "URL:Dogecoin"
    WriteRegStr HKCR "dogecoin\DefaultIcon" "" $INSTDIR\dogecoin-qt.exe
    WriteRegStr HKCR "dogecoin\shell\open\command" "" '"$INSTDIR\dogecoin-qt.exe" "%1"'
SectionEnd

# Macro for selecting uninstaller sections
!macro SELECT_UNSECTION SECTION_NAME UNSECTION_ID
    Push $R0
    ReadRegStr $R0 HKCU "${REGKEY}\Components" "${SECTION_NAME}"
    StrCmp $R0 1 0 next${UNSECTION_ID}
    !insertmacro SelectSection "${UNSECTION_ID}"
    GoTo done${UNSECTION_ID}
next${UNSECTION_ID}:
    !insertmacro UnselectSection "${UNSECTION_ID}"
done${UNSECTION_ID}:
    Pop $R0
!macroend

# Uninstaller sections
Section /o -un.Main UNSEC0000
    Delete /REBOOTOK $INSTDIR\dogecoin-qt.exe
    Delete /REBOOTOK $INSTDIR\COPYING.txt
    Delete /REBOOTOK $INSTDIR\readme.txt
    RMDir /r /REBOOTOK $INSTDIR\daemon
    RMDir /r /REBOOTOK $INSTDIR\doc
    DeleteRegValue HKCU "${REGKEY}\Components" Main
SectionEnd

Section -un.post UNSEC0001
    DeleteRegKey HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Uninstall $(^Name).lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\$(^Name).lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core (testnet, 64-bit).lnk"
    Delete /REBOOTOK "$SMSTARTUP\Dogecoin.lnk"
    Delete /REBOOTOK $INSTDIR\uninstall.exe
    Delete /REBOOTOK $INSTDIR\debug.log
    Delete /REBOOTOK $INSTDIR\db.log
    DeleteRegValue HKCU "${REGKEY}" StartMenuGroup
    DeleteRegValue HKCU "${REGKEY}" Path
    DeleteRegKey /IfEmpty HKCU "${REGKEY}\Components"
    DeleteRegKey /IfEmpty HKCU "${REGKEY}"
    DeleteRegKey HKCR "dogecoin"
    RmDir /REBOOTOK $SMPROGRAMS\$StartMenuGroup
    RmDir /REBOOTOK $INSTDIR
    Push $R0
    StrCpy $R0 $StartMenuGroup 1
    StrCmp $R0 ">" no_smgroup
no_smgroup:
    Pop $R0
SectionEnd

# Installer functions
Function .onInit
    InitPluginsDir
!if "64" == "64"
    ${If} ${RunningX64}
      ; disable registry redirection (enable access to 64-bit portion of registry)
      SetRegView 64
    ${Else}
      MessageBox MB_OK|MB_ICONSTOP "Cannot install 64-bit version on a 32-bit system."
      Abort
    ${EndIf}
!endif
    ; Default Core datadir on Windows
    ReadEnvStr $0 "APPDATA"
    ${If} $0 == ""
      StrCpy $DogecoinDataDir "$PROFILE\AppData\Roaming\Dogecoin"
    ${Else}
      StrCpy $DogecoinDataDir "$0\Dogecoin"
    ${EndIf}
    ; Unique password every install - never a shared default
    StrCpy $RpcUser "dogecoin"
    Call GenerateRpcPassword
    ; Override default user for Core GUI installs
    StrCpy $RpcUser "dogecoin"
FunctionEnd

# Uninstaller functions
Function un.onInit
    ReadRegStr $INSTDIR HKCU "${REGKEY}" Path
    !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuGroup
    !insertmacro SELECT_UNSECTION Main ${UNSEC0000}
FunctionEnd
