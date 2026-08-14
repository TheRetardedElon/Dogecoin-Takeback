Name "Dogecoin Core Pro (64-bit)"

RequestExecutionLevel highest
SetCompressor /SOLID lzma

# General Symbol Definitions
!define REGKEY "SOFTWARE\$(^Name)"
!ifndef VERSION
!define VERSION 1.14.105
!endif
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
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Dogecoin Core Pro"
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchInstalledApp
!define MUI_FINISHPAGE_RUN_TEXT "Start Dogecoin Core Pro"
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
Var InstallRole
Var PurgeDataDir
Var PurgeDlg
Var PurgeCheck

# Unique RPC password page (shared with GPENode installer pattern)
!define GEN_RPC_PS1 "/mnt/c/dogedev/share/gen-rpc-password.ps1"
!include "/mnt/c/dogedev/share/nsis-rpc-credentials.nsh"
!include "/mnt/c/dogedev/share/nsis-install-role.nsh"
ReserveFile "/mnt/c/dogedev/share/gen-rpc-password.ps1"

# Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom InstallRolePage InstallRolePageLeave
Page custom RpcCredentialsPage RpcCredentialsPageLeave
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuGroup
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
UninstPage custom un.PurgePage un.PurgePageLeave
!insertmacro MUI_UNPAGE_INSTFILES

# Installer languages
!insertmacro MUI_LANGUAGE English

# Installer attributes
OutFile /mnt/c/dogedev/release/dogecoin-1.14.105-win64-setup-rpcsecure.exe
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
VIAddVersionKey ProductName "Dogecoin Core Pro"
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
    File /oname=COPYING.txt /mnt/c/dogedev/COPYING
    File /oname=readme.txt /mnt/c/dogedev/doc/README_windows.txt
    File "/mnt/c/dogedev/share/launch-hybrid.ps1"
!if /FileExists "/mnt/c/dogedev/pro-gui/build-win/corepro-launch.exe"
    File /mnt/c/dogedev/pro-gui/build-win/corepro-launch.exe
!endif
    File "/mnt/c/dogedev/doc/install-roles.md"
!if /FileExists "/mnt/c/dogedev/share/pixmaps/dogecoin-testnet.ico"
    File /oname=dogecoin-testnet.ico "/mnt/c/dogedev/share/pixmaps/dogecoin-testnet.ico"
!endif
    ; One daemon for every role
    SetOutPath $INSTDIR\daemon
    File /mnt/c/dogedev/release/dogecoind.exe
    File /mnt/c/dogedev/release/dogecoin-cli.exe
    SetOutPath $INSTDIR
    ; Client / Hybrid desktop GUI (ImGui). Qt is not shipped.
!if /FileExists "/mnt/c/dogedev/release/smoke-pro-gui/dogecoin-pro-gui-smoke.exe"
    File /oname=dogecoin-pro-gui.exe /mnt/c/dogedev/release/smoke-pro-gui/dogecoin-pro-gui-smoke.exe
!endif
!if /FileExists "/mnt/c/dogedev/pro-gui/build-win/dogecoin-pro-gui.exe"
    File /mnt/c/dogedev/pro-gui/build-win/dogecoin-pro-gui.exe
!endif
!if /FileExists "/mnt/c/dogedev/release/smoke-pro-gui/dogeinit.exe"
    File /mnt/c/dogedev/release/smoke-pro-gui/dogeinit.exe
!endif
!if /FileExists "/mnt/c/dogedev/release/smoke-pro-gui/WebView2Loader.dll"
    File /mnt/c/dogedev/release/smoke-pro-gui/WebView2Loader.dll
!endif
    ; ImGui assets next to the GUI
!if /FileExists "/mnt/c/dogedev/release/smoke-pro-gui/assets/manifest.json"
    SetOutPath $INSTDIR\assets
    File /r "/mnt/c/dogedev/release/smoke-pro-gui/assets/"
    SetOutPath $INSTDIR
!endif
    ; Server / Hybrid operator TUI (from GPENode tree when present)
!if /FileExists "/mnt/c/dogedevGPEnode/gpenode-tui/gpenode-tui.exe"
    File /mnt/c/dogedevGPEnode/gpenode-tui/gpenode-tui.exe
!endif
!if /FileExists "/mnt/c/dogedevGPEnode/gpenode-ops/gpenode-ops.exe"
    File /mnt/c/dogedevGPEnode/gpenode-ops/gpenode-ops.exe
!endif
!if /FileExists "/mnt/c/dogedevGPEnode/gpenode-tray/gpenode-tray.exe"
    File /mnt/c/dogedevGPEnode/gpenode-tray/gpenode-tray.exe
!endif
    SetOutPath $INSTDIR\doc
    File /mnt/c/dogedev/doc/install-roles.md
    File /mnt/c/dogedev/doc/README_windows.txt
    SetOutPath $INSTDIR
    ; Never leave Qt around — Windows Start indexes dogecoin-qt.exe as "Dogecoin Core".
    Delete /REBOOTOK $INSTDIR\dogecoin-qt.exe
    Delete /REBOOTOK $INSTDIR\dogecoin-qt
    ; Installed footprint follows the chosen role (setup.exe still carries all UIs).
    ${If} $InstallRole == "server"
        Delete /REBOOTOK $INSTDIR\dogecoin-pro-gui.exe
    ${EndIf}
    ${If} $InstallRole == "client"
        Delete /REBOOTOK $INSTDIR\gpenode-tui.exe
        Delete /REBOOTOK $INSTDIR\gpenode-ops.exe
        Delete /REBOOTOK $INSTDIR\gpenode-tray.exe
        Delete /REBOOTOK $INSTDIR\launch-hybrid.ps1
    ${EndIf}
    FileOpen $0 $INSTDIR\install-role.txt w
    FileWrite $0 $InstallRole
    FileClose $0
    ${If} $InstallRole == "hybrid"
        IfFileExists $INSTDIR\hybrid-ui.txt skip_hybrid_pref
        FileOpen $0 $INSTDIR\hybrid-ui.txt w
        FileWrite $0 "ask"
        FileClose $0
        skip_hybrid_pref:
    ${EndIf}
    WriteRegStr HKCU "${REGKEY}\Components" Main 1
    WriteRegStr HKCU "${REGKEY}" InstallRole $InstallRole
SectionEnd

Section -post SEC0001
    WriteRegStr HKCU "${REGKEY}" Path $INSTDIR
    SetOutPath $INSTDIR
    WriteUninstaller $INSTDIR\uninstall.exe
    ; Unique RPC credentials helper scripts
    File "/mnt/c/dogedev/share/write-install-conf.ps1"
    File "/mnt/c/dogedev/share/gen-rpc-password.ps1"
    File "/mnt/c/dogedev/share/install-corepro-service.ps1"
    File "/mnt/c/dogedev/share/stop-dogecoind.ps1"
    CreateDirectory $INSTDIR\tor
    File /oname=tor\README.txt "/mnt/c/dogedev/share/tor-README.txt"
    ; Write dogecoin.conf into %APPDATA%\Dogecoin if missing (unique password per install)
    CreateDirectory "$DogecoinDataDir"
    StrCpy $R9 "core"
    ${If} $InstallRole == "server"
        StrCpy $R9 "dump"
    ${EndIf}
    ${If} $InstallRole == "hybrid"
        StrCpy $R9 "hybrid"
    ${EndIf}
    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\write-install-conf.ps1" -DataDir "$DogecoinDataDir" -RpcUser "$RpcUser" -RpcPassword "$RpcPassword" -Profile "$R9"'
    Pop $1
    DetailPrint "write-install-conf profile=$R9 exit=$1"
    ; Hybrid/Server: one Windows service wrapping dogecoind (gpenode-ops service-run).
    ; Skip start if dogecoind is already running so we never attach two nodes to one datadir.
    ${If} $InstallRole == "server"
    ${OrIf} $InstallRole == "hybrid"
        nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\install-corepro-service.ps1" -BinDir "$INSTDIR\daemon" -OpsPath "$INSTDIR\gpenode-ops.exe" -DataDir "$DogecoinDataDir" -ConfFile "$DogecoinDataDir\dogecoin.conf" -StartIfIdle'
        Pop $1
        DetailPrint "install-corepro-service exit=$1"
    ${EndIf}
    !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    SetShellVarContext all
    CreateDirectory $SMPROGRAMS\$StartMenuGroup
    ; Drop leftover Qt Start Menu names from older installs.
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\$(^Name).lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\Dogecoin Core\Dogecoin Core.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\Dogecoin Core\Dogecoin Core (64-bit).lnk"
    Delete /REBOOTOK "$DESKTOP\Dogecoin Core.lnk"
    Delete /REBOOTOK "$DESKTOP\Dogecoin Core Pro.lnk"
    ; Older Hybrid installs put a second tray (gpenode-tray) in Startup.
    ; The Desktop GUI owns the one notify icon; do not relaunch the helper.
    Delete /REBOOTOK "$SMSTARTUP\Dogecoin Core Pro tray.lnk"
    Delete /REBOOTOK "$SMSTARTUP\Dogecoin.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Hybrid tray.lnk"
    ${If} $InstallRole == "server"
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Operator TUI.lnk" $INSTDIR\gpenode-tui.exe
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Operator TUI Testnet.lnk" $INSTDIR\gpenode-tui.exe "--testnet" "$INSTDIR\dogecoin-testnet.ico" 0
    ${ElseIf} $InstallRole == "hybrid"
        ; Primary Start hit is the launcher. Do NOT ship a top-level "Desktop GUI"
        ; shortcut — Windows Search ranks that first and skips the Hybrid picker.
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core Pro.lnk" $INSTDIR\corepro-launch.exe
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core.lnk" $INSTDIR\corepro-launch.exe
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core Pro Testnet.lnk" $INSTDIR\corepro-launch.exe "--testnet" "$INSTDIR\dogecoin-testnet.ico" 0
        Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Desktop GUI.lnk"
        Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Operator TUI.lnk"
        Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Choose UI (ask).lnk"
        Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Hybrid tray.lnk"
        Delete /REBOOTOK "$SMSTARTUP\Dogecoin Core Pro tray.lnk"
        CreateShortcut "$DESKTOP\Dogecoin Core Pro.lnk" $INSTDIR\corepro-launch.exe
        CreateShortcut "$DESKTOP\Dogecoin Core Pro Testnet.lnk" $INSTDIR\corepro-launch.exe "--testnet" "$INSTDIR\dogecoin-testnet.ico" 0
    ${Else}
        ; Client: same launcher as Hybrid so we never pin a console or a build-tree exe.
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core Pro.lnk" $INSTDIR\corepro-launch.exe
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core.lnk" $INSTDIR\corepro-launch.exe
        CreateShortcut "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core Pro Testnet.lnk" $INSTDIR\corepro-launch.exe "--testnet" "$INSTDIR\dogecoin-testnet.ico" 0
        CreateShortcut "$DESKTOP\Dogecoin Core Pro.lnk" $INSTDIR\corepro-launch.exe
        CreateShortcut "$DESKTOP\Dogecoin Core Pro Testnet.lnk" $INSTDIR\corepro-launch.exe "--testnet" "$INSTDIR\dogecoin-testnet.ico" 0
    ${EndIf}
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
    WriteRegStr HKCR "dogecoin\DefaultIcon" "" $INSTDIR\dogecoin-pro-gui.exe
    WriteRegStr HKCR "dogecoin\shell\open\command" "" '"$INSTDIR\dogecoin-pro-gui.exe" "%1"'
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
Section -un.StopNode UNSECSTOP
    DetailPrint "Stopping dogecoind and the Windows service before removing files..."
    IfFileExists "$INSTDIR\stop-dogecoind.ps1" 0 un_stop_inline
    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\stop-dogecoind.ps1" -WaitSeconds 90 -DataDir "$DogecoinDataDir"'
    Pop $1
    DetailPrint "stop-dogecoind exit=$1"
    Goto un_stop_done
un_stop_inline:
    nsExec::ExecToLog 'sc.exe stop DogecoinGPENode'
    Pop $1
    nsExec::ExecToLog 'taskkill.exe /IM dogecoind.exe /T'
    Pop $1
un_stop_done:
    nsExec::ExecToLog 'sc.exe delete DogecoinGPENode'
    Pop $1
SectionEnd

Section /o -un.Main UNSEC0000
    Delete /REBOOTOK $INSTDIR\dogecoin-qt.exe
    Delete /REBOOTOK $INSTDIR\dogecoin-pro-gui.exe
    Delete /REBOOTOK $INSTDIR\dogeinit.exe
    Delete /REBOOTOK $INSTDIR\WebView2Loader.dll
    Delete /REBOOTOK $INSTDIR\gpenode-tui.exe
    Delete /REBOOTOK $INSTDIR\gpenode-ops.exe
    Delete /REBOOTOK $INSTDIR\gpenode-tray.exe
    Delete /REBOOTOK $INSTDIR\launch-hybrid.ps1
    Delete /REBOOTOK $INSTDIR\corepro-launch.exe
    Delete /REBOOTOK $INSTDIR\dogecoin-testnet.ico
    Delete /REBOOTOK $INSTDIR\write-install-conf.ps1
    Delete /REBOOTOK $INSTDIR\gen-rpc-password.ps1
    Delete /REBOOTOK $INSTDIR\install-corepro-service.ps1
    Delete /REBOOTOK $INSTDIR\stop-dogecoind.ps1
    Delete /REBOOTOK $INSTDIR\install-role.txt
    Delete /REBOOTOK $INSTDIR\hybrid-ui.txt
    Delete /REBOOTOK $INSTDIR\install-roles.md
    Delete /REBOOTOK $INSTDIR\COPYING.txt
    Delete /REBOOTOK $INSTDIR\readme.txt
    RMDir /r /REBOOTOK $INSTDIR\daemon
    RMDir /r /REBOOTOK $INSTDIR\doc
    RMDir /r /REBOOTOK $INSTDIR\assets
    DeleteRegValue HKCU "${REGKEY}\Components" Main
SectionEnd

Section -un.post UNSEC0001
    ${If} $PurgeDataDir == "1"
        DetailPrint "FULL PURGE: removing $DogecoinDataDir (blocks, chainstate, wallet)"
        RMDir /r /REBOOTOK $DogecoinDataDir
        ReadEnvStr $R8 "ProgramData"
        ${If} $R8 != ""
            RMDir /r /REBOOTOK "$R8\DogecoinGPENode"
            RMDir /r /REBOOTOK "$R8\DogecoinCorePro"
        ${EndIf}
    ${Else}
        DetailPrint "Data left at $DogecoinDataDir (can be many GB). Check FULL PURGE next time to free the disk."
    ${EndIf}
    DeleteRegKey HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)"
    SetShellVarContext all
    Delete /REBOOTOK "$DESKTOP\Dogecoin Core Pro.lnk"
    Delete /REBOOTOK "$DESKTOP\Dogecoin Core Pro Testnet.lnk"
    Delete /REBOOTOK "$DESKTOP\Dogecoin Core.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Uninstall $(^Name).lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\$(^Name).lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core Pro.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core Pro Testnet.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Operator TUI Testnet.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core Pro (choose UI).lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Desktop GUI.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Operator TUI.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Hybrid tray.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Operator TUI.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\RPC credentials.lnk"
    Delete /REBOOTOK "$SMPROGRAMS\$StartMenuGroup\Dogecoin Core (testnet, 64-bit).lnk"
    Delete /REBOOTOK "$SMSTARTUP\Dogecoin.lnk"
    Delete /REBOOTOK "$SMSTARTUP\Dogecoin Core Pro tray.lnk"
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
    StrCpy $InstallRole "client"
FunctionEnd

Function LaunchInstalledApp
    ${If} $InstallRole == "server"
        IfFileExists $INSTDIR\gpenode-tui.exe 0 +3
            Exec '"$INSTDIR\gpenode-tui.exe"'
            Return
        Exec '"$INSTDIR\daemon\dogecoind.exe" -server'
    ${ElseIf} $InstallRole == "hybrid"
        IfFileExists $INSTDIR\corepro-launch.exe 0 +3
            Exec '"$INSTDIR\corepro-launch.exe"'
            Return
        Exec '"$INSTDIR\dogecoin-pro-gui.exe" --ui gfx'
    ${Else}
        IfFileExists $INSTDIR\corepro-launch.exe 0 +3
            Exec '"$INSTDIR\corepro-launch.exe"'
            Return
        IfFileExists $INSTDIR\dogecoin-pro-gui.exe 0 +2
            Exec '"$INSTDIR\dogecoin-pro-gui.exe" --ui gfx'
    ${EndIf}
FunctionEnd

# Uninstaller functions
Function un.onInit
    ReadRegStr $INSTDIR HKCU "${REGKEY}" Path
    !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuGroup
    !insertmacro SELECT_UNSECTION Main ${UNSEC0000}
    ReadEnvStr $0 "APPDATA"
    ${If} $0 == ""
      StrCpy $DogecoinDataDir "$PROFILE\AppData\Roaming\Dogecoin"
    ${Else}
      StrCpy $DogecoinDataDir "$0\Dogecoin"
    ${EndIf}
    StrCpy $PurgeDataDir "0"
FunctionEnd

Function un.PurgePage
    !insertmacro MUI_HEADER_TEXT "Blockchain data" "The node files can use a very large amount of disk if you leave them."
    nsDialogs::Create 1018
    Pop $PurgeDlg
    ${NSD_CreateLabel} 0 0 100% 36u \
        "Uninstall removes the program. By default your blockchain, chainstate, and wallet stay on disk at:$\r$\n$DogecoinDataDir"
    Pop $0
    ${NSD_CreateLabel} 0 40u 100% 28u \
        "If you do not delete that folder, tens of gigabytes can sit unused. Only check FULL PURGE if you want a clean wipe (wallet.dat is deleted too)."
    Pop $0
    ${NSD_CreateCheckbox} 0 78u 100% 18u "FULL PURGE — delete blocks, chainstate, and wallet data"
    Pop $PurgeCheck
    ${NSD_Uncheck} $PurgeCheck
    nsDialogs::Show
FunctionEnd

Function un.PurgePageLeave
    ${NSD_GetState} $PurgeCheck $0
    ${If} $0 == ${BST_CHECKED}
        MessageBox MB_YESNO|MB_ICONEXCLAMATION \
            "FULL PURGE deletes your wallet.dat and the entire blockchain folder.$\r$\n$\r$\nThis cannot be undone. Continue?" \
            IDYES purge_yes
        Abort
        purge_yes:
        StrCpy $PurgeDataDir "1"
    ${Else}
        StrCpy $PurgeDataDir "0"
    ${EndIf}
FunctionEnd
