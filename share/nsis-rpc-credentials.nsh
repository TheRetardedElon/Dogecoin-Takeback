; Unique RPC password page for Windows installers (GPENode / Core Pro).
; Requires: nsDialogs, LogicLib, nsExec
; Vars (declare in parent .nsi BEFORE this include):
;   RpcPassword, RpcUser, RpcAckCheckbox, RpcDlg, RpcPassEdit
;
; Parent .nsi should also:
;   ReserveFile "gen-rpc-password.ps1"

!include "nsDialogs.nsh"

; ---------------------------------------------------------------------------
; Cryptographically strong password via gen-rpc-password.ps1 extracted to
; $PLUGINSDIR (available during .onInit). Never use a fixed suffix fallback.
; ---------------------------------------------------------------------------
Function GenerateRpcPassword
  ; Default user (parent may overwrite after Call)
  StrCpy $RpcUser "gpenode"

  InitPluginsDir
  ; Extract generator next to plugins (works in .onInit before install)
  !ifndef GEN_RPC_PS1
    !define GEN_RPC_PS1 "gen-rpc-password.ps1"
  !endif
  File /oname=$PLUGINSDIR\gen-rpc-password.ps1 "${GEN_RPC_PS1}"

  ; Full path to powershell
  StrCpy $R0 "$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
  IfFileExists $R0 +2 0
    StrCpy $R0 "powershell.exe"

  nsExec::ExecToStack '"$R0" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\gen-rpc-password.ps1"'
  Pop $R1  ; exit code
  Pop $R2  ; stdout

  ${If} $R1 == 0
  ${AndIf} $R2 != ""
    Push $R2
    Call StripNewlines
    Pop $RpcPassword
    ; sanity: at least 16 chars
    StrLen $R3 $RpcPassword
    IntCmp $R3 16 gen_ok gen_fallback gen_ok
  ${EndIf}

gen_fallback:
  ; Stronger multi-source fallback (still unique; no fixed "X7kQ2mN9pL" tail)
  ; Uses RtlGenRandom when available, else mixes several entropy sources.
  Call GenerateRpcPasswordRtl
  StrCmp $RpcPassword "" 0 gen_ok
  Call GenerateRpcPasswordMixed
gen_ok:
FunctionEnd

; RtlGenRandom / SystemFunction036 — 24 random bytes -> base62 string
Function GenerateRpcPasswordRtl
  StrCpy $RpcPassword ""
  System::Alloc 32
  Pop $R0
  StrCmp $R0 "0" rtl_fail
  ; BOOLEAN SystemFunction036(PVOID buf, ULONG len)
  System::Call "advapi32::SystemFunction036(i R0, i 24) i.r1"
  IntCmp $R1 0 rtl_free_fail
  ; Map 24 bytes to alphanum via a simple loop reading memory
  ; System::Call to copy each byte
  StrCpy $R4 0
  StrCpy $R5 ""
rtl_loop:
  IntCmp $R4 24 rtl_done
  System::Call "*$R0(&i1 .r6)"
  IntOp $R0 $R0 + 1
  IntOp $R6 $R6 % 62
  ; charset index $R6 -> char via StrCpy substr
  StrCpy $R7 "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" 1 $R6
  StrCpy $R5 "$R5$R7"
  IntOp $R4 $R4 + 1
  Goto rtl_loop
rtl_done:
  ; rewind free base — we advanced R0; free original
  IntOp $R0 $R0 - 24
  System::Free $R0
  StrCpy $RpcPassword $R5
  Return
rtl_free_fail:
  System::Free $R0
rtl_fail:
  StrCpy $RpcPassword ""
FunctionEnd

; Last-resort mixed entropy (tick, pid, perf counter, GUID via powershell one-liner)
Function GenerateRpcPasswordMixed
  System::Call 'kernel32::GetTickCount()i.r5'
  System::Call 'kernel32::GetCurrentProcessId()i.r4'
  System::Call 'kernel32::QueryPerformanceCounter(*l .r3)'
  ; powershell Guid is very reliable as extra entropy
  StrCpy $R0 "$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
  nsExec::ExecToStack '"$R0" -NoProfile -NonInteractive -Command "[guid]::NewGuid().ToString(\"N\")+(Get-Date).Ticks"'
  Pop $R1
  Pop $R2
  Push $R2
  Call StripNewlines
  Pop $R2
  ; Build password from mixed sources — all variable, no fixed suffix
  StrCpy $RpcPassword "G$5$4$R2"
  ; Truncate / pad to ~28 chars from charset scramble
  StrCpy $R8 "$RpcPassword$3$5$4"
  StrLen $R9 $R8
  ; If still short, append more ticks
  IntCmp $R9 20 mix_ok mix_pad mix_ok
mix_pad:
  System::Call 'kernel32::GetTickCount()i.r5'
  StrCpy $R8 "$R8$5extraEntropySeed"
mix_ok:
  ; Take 28 chars starting at offset 0
  StrCpy $RpcPassword $R8 28
FunctionEnd

Function StripNewlines
  Exch $0
  Push $1
strip_loop:
  StrCpy $1 $0 1 -1
  StrCmp $1 "$\r" strip_chop
  StrCmp $1 "$\n" strip_chop
  StrCmp $1 " " strip_chop
  StrCmp $1 "$\t" strip_chop
  Goto strip_done
strip_chop:
  StrCpy $0 $0 -1
  Goto strip_loop
strip_done:
  Pop $1
  Exch $0
FunctionEnd

Function RpcCredentialsPage
  !insertmacro MUI_HEADER_TEXT "Unique RPC password" "Each install gets its own password - no shared default."

  nsDialogs::Create 1018
  Pop $RpcDlg

  ${NSD_CreateLabel} 0 0u 100% 32u "A random RPC password was generated for this installation only.$\r$\nIt will be written to dogecoin.conf and RPC-CREDENTIALS.txt in the data folder.$\r$\nRPC remains on 127.0.0.1 - still keep this secret."
  Pop $0

  ${NSD_CreateLabel} 0 36u 100% 10u "rpcuser"
  Pop $0
  ${NSD_CreateText} 0 46u 100% 12u "$RpcUser"
  Pop $0
  EnableWindow $0 0

  ${NSD_CreateLabel} 0 64u 100% 10u "rpcpassword  (select all and copy - Ctrl+A, Ctrl+C)"
  Pop $0
  ${NSD_CreateText} 0 74u 100% 12u "$RpcPassword"
  Pop $RpcPassEdit

  ${NSD_CreateCheckbox} 0 100u 100% 18u "I have saved this password (or know it will be in the data folder files)."
  Pop $RpcAckCheckbox

  ${NSD_CreateLabel} 0 122u 100% 28u "You must check the box to continue.$\r$\nThis stops every install from sharing a default password like CHANGE_ME."
  Pop $0

  nsDialogs::Show
FunctionEnd

Function RpcCredentialsPageLeave
  ${NSD_GetState} $RpcAckCheckbox $0
  ${If} $0 == 0
    MessageBox MB_OK|MB_ICONEXCLAMATION "Please check the box confirming you saved the RPC password."
    Abort
  ${EndIf}
  ${NSD_GetText} $RpcPassEdit $RpcPassword
  StrCmp $RpcPassword "" 0 +3
    MessageBox MB_OK|MB_ICONSTOP "Password cannot be empty."
    Abort
  ; Reject known-weak fallback pattern if user somehow still has it
  StrCpy $R0 $RpcPassword 3
  StrCmp $R0 "Gpe" 0 leave_ok
  StrCpy $R0 $RpcPassword "" -10
  StrCmp $R0 "X7kQ2mN9pL" 0 leave_ok
    MessageBox MB_OK|MB_ICONSTOP "Password looks like a failed generator fallback. Close Setup and re-run, or paste a long random password."
    Abort
leave_ok:
FunctionEnd
