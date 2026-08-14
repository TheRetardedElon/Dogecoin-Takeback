; Client / Server / Hybrid role page.
; Parent must declare: Var InstallRole
; Values: client | server | hybrid  (default client)

!include "nsDialogs.nsh"

Var RoleDlg
Var RoleClient
Var RoleServer
Var RoleHybrid
Var RoleHint

Function InstallRolePage
    nsDialogs::Create 1018
    Pop $RoleDlg
    ${If} $RoleDlg == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 28u \
        "How are you installing Dogecoin Core Pro?  One dogecoind is installed in every case."
    Pop $0

    ${NSD_CreateRadioButton} 0 32u 100% 14u "Client  -  desktop wallet (ImGui)"
    Pop $RoleClient
    ${NSD_CreateRadioButton} 0 50u 100% 14u "Server  -  headless operator (TUI only, no desktop GUI)"
    Pop $RoleServer
    ${NSD_CreateRadioButton} 0 68u 100% 14u "Hybrid  -  both UIs, still one dogecoind"
    Pop $RoleHybrid

    ${NSD_CreateLabel} 0 92u 100% 36u \
        "Hybrid: the tray asks Desktop GUI or Operator TUI. They share the same node and datadir. Never run two daemons."
    Pop $RoleHint

    ${If} $InstallRole == "server"
        ${NSD_Check} $RoleServer
    ${ElseIf} $InstallRole == "hybrid"
        ${NSD_Check} $RoleHybrid
    ${Else}
        ${NSD_Check} $RoleClient
        StrCpy $InstallRole "client"
    ${EndIf}

    nsDialogs::Show
FunctionEnd

Function InstallRolePageLeave
    ${NSD_GetState} $RoleClient $0
    ${NSD_GetState} $RoleServer $1
    ${NSD_GetState} $RoleHybrid $2
    ${If} $1 == ${BST_CHECKED}
        StrCpy $InstallRole "server"
    ${ElseIf} $2 == ${BST_CHECKED}
        StrCpy $InstallRole "hybrid"
    ${Else}
        StrCpy $InstallRole "client"
    ${EndIf}
FunctionEnd
