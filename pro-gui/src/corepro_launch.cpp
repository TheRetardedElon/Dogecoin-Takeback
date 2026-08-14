// corepro-launch — WIN32 subsystem (no console). Hybrid picker + start GUI/TUI.
// Replaces PowerShell launch-hybrid.ps1 as the Start Menu / Desktop target.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsvc.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <cctype>

static const int kW = 500;
static const int kH = 300;
static const int ID_GUI = 101;
static const int ID_TUI = 102;
static const int ID_REM = 103;
static const int ID_OK = 104;
static const int ID_CANCEL = 105;

static std::string g_dir;
static bool g_remember = false;
static bool g_wantTui = false;
static bool g_testnet = false;
static bool g_regtest = false;

static std::string Join(const std::string& a, const char* b)
{
    if (a.empty())
        return b;
    char c = a.back();
    if (c == '\\' || c == '/')
        return a + b;
    return a + "\\" + b;
}

static std::string ExeDir()
{
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    auto s = p.find_last_of("\\/");
    return (s == std::string::npos) ? "." : p.substr(0, s);
}

static bool Exists(const std::string& p)
{
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string ReadFirstToken(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
        return {};
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            i++;
        if (i >= line.size() || line[i] == '#')
            continue;
        std::string v = line.substr(i);
        for (char& c : v) {
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');
        }
        return v;
    }
    return {};
}

static std::string Role()
{
    std::string v = ReadFirstToken(Join(g_dir, "install-role.txt"));
    if (v == "client" || v == "server" || v == "hybrid")
        return v;
    return "client";
}

static std::string HybridPref()
{
    std::string v = ReadFirstToken(Join(g_dir, "hybrid-ui.txt"));
    if (v == "gfx" || v == "gui" || v == "imgui" || v == "desktop")
        return "gfx";
    if (v == "tui" || v == "operator" || v == "gpenode")
        return "tui";
    return "ask";
}

static void WriteHybridPref(const char* v)
{
    std::ofstream f(Join(g_dir, "hybrid-ui.txt"), std::ios::trunc);
    if (!f)
        return;
    f << "# Hybrid default UI. ask | gfx | tui\n" << v << "\n";
}

static bool ProcRunning(const char* exeName)
{
    // Best-effort: CreateToolhelp is heavier; use FindWindow / OpenProcess via snapshot.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W pe {};
    pe.dwSize = sizeof(pe);
    wchar_t wname[MAX_PATH] = {};
    MultiByteToWideChar(CP_ACP, 0, exeName, -1, wname, MAX_PATH);
    bool hit = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, wname) == 0) {
                hit = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return hit;
}

static bool StartDetached(const std::string& exe, const std::string& args, bool newConsole)
{
    if (!Exists(exe))
        return false;
    std::string cmd = "\"" + exe + "\"";
    if (!args.empty())
        cmd += " " + args;
    STARTUPINFOA si {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi {};
    DWORD flags = newConsole ? CREATE_NEW_CONSOLE : (CREATE_NO_WINDOW | DETACHED_PROCESS);
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    BOOL ok = CreateProcessA(exe.c_str(), buf.data(), nullptr, nullptr, FALSE, flags,
                             nullptr, g_dir.c_str(), &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return ok != 0;
}

static bool StartCoreProService()
{
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return false;
    SC_HANDLE svc = OpenServiceA(scm, "DogecoinGPENode", SERVICE_START | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }
    BOOL started = StartServiceA(svc, 0, nullptr);
    const DWORD err = GetLastError();
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return started || err == ERROR_SERVICE_ALREADY_RUNNING;
}

static std::string NetworkArgs()
{
    if (g_testnet)
        return " --testnet";
    if (g_regtest)
        return " --regtest";
    return {};
}

static void ParseCmdLine(const char* lpCmdLine)
{
    std::string src;
    if (lpCmdLine && lpCmdLine[0])
        src = lpCmdLine;
    const char* full = GetCommandLineA();
    if (full && full[0])
        src = full;
    auto lower = src;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    }
    auto hasTok = [&](const char* tok) {
        const std::string t(tok);
        size_t pos = 0;
        while ((pos = lower.find(t, pos)) != std::string::npos) {
            const bool left = (pos == 0) || lower[pos - 1] == ' ' || lower[pos - 1] == '"';
            const size_t end = pos + t.size();
            const bool right = (end >= lower.size()) || lower[end] == ' ' || lower[end] == '"';
            if (left && right)
                return true;
            pos += t.size();
        }
        return false;
    };
    if (hasTok("--testnet") || hasTok("-testnet"))
        g_testnet = true;
    if (hasTok("--regtest") || hasTok("-regtest"))
        g_regtest = true;
    if (hasTok("--mainnet") || hasTok("-mainnet")) {
        g_testnet = false;
        g_regtest = false;
    }
}

static void StartNodeIfNeeded()
{
    // Testnet / regtest never touch the mainnet Windows service. The GUI/TUI
    // start a separate dogecoind with -testnet / -regtest on the network datadir.
    if (g_testnet || g_regtest)
        return;
    if (ProcRunning("dogecoind.exe"))
        return;
    const std::string role = Role();
    // Hybrid/Server: the Windows service owns dogecoind (no console).
    if (role == "hybrid" || role == "server") {
        if (StartCoreProService())
            return;
        // Service exists on Hybrid/Server. Do not spawn an unsupervised
        // dogecoind — that orphan ignores SCM stop and looks like a second node.
        if (Exists(Join(g_dir, "gpenode-ops.exe")))
            return;
    }
    std::string d = Join(g_dir, "daemon\\dogecoind.exe");
    if (!Exists(d))
        d = Join(g_dir, "dogecoind.exe");
    if (Exists(d))
        StartDetached(d, "-server", false);
}

static bool StartGui()
{
    std::string g = Join(g_dir, "dogecoin-pro-gui.exe");
    if (!Exists(g))
        g = Join(g_dir, "dogecoin-pro-gui-smoke.exe");
    return StartDetached(g, std::string("--ui gfx") + NetworkArgs(), false);
}

static bool StartTui()
{
    return StartDetached(Join(g_dir, "gpenode-tui.exe"), std::string("--ui tui") + NetworkArgs(), true);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        CreateWindowA("STATIC",
                      g_testnet
                          ? "TESTNET — coins are worthless. Same picker; different chain.\n"
                            "One dogecoind. Which UI?"
                          : "Hybrid = this PC is a full Dogecoin node plus a desktop wallet.\n"
                            "One dogecoind. Which UI?",
                      WS_CHILD | WS_VISIBLE, 16, 16, 460, 48, h, nullptr, nullptr, nullptr);
        CreateWindowA("BUTTON", "Desktop GUI  (wallet on this node)",
                      WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                      24, 78, 440, 22, h, (HMENU)(INT_PTR)ID_GUI, nullptr, nullptr);
        CreateWindowA("BUTTON", "Operator TUI  (service, dump, CDN)",
                      WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                      24, 106, 440, 22, h, (HMENU)(INT_PTR)ID_TUI, nullptr, nullptr);
        CheckRadioButton(h, ID_GUI, ID_TUI, ID_GUI);
        CreateWindowA("BUTTON", "Remember this (do not ask next time)",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      24, 142, 440, 22, h, (HMENU)(INT_PTR)ID_REM, nullptr, nullptr);
        CreateWindowA("STATIC",
                      "X sends the UI to the tray (node stays up). File, Exit / tray Quit stop the node.",
                      WS_CHILD | WS_VISIBLE, 16, 176, 460, 36, h, nullptr, nullptr, nullptr);
        CreateWindowA("BUTTON", "Open", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      170, 228, 88, 28, h, (HMENU)(INT_PTR)ID_OK, nullptr, nullptr);
        CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
                      270, 228, 88, 28, h, (HMENU)(INT_PTR)ID_CANCEL, nullptr, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == ID_OK) {
            g_wantTui = (IsDlgButtonChecked(h, ID_TUI) == BST_CHECKED);
            g_remember = (IsDlgButtonChecked(h, ID_REM) == BST_CHECKED);
            DestroyWindow(h);
            return 0;
        }
        if (id == ID_CANCEL) {
            PostQuitMessage(1);
            DestroyWindow(h);
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        PostQuitMessage(1);
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(h, m, w, l);
    }
}

static int AskHybrid()
{
    WNDCLASSA wc {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CoreProHybridPick";
    wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(1));
    if (g_testnet) {
        std::string ico = Join(g_dir, "dogecoin-testnet.ico");
        if (!Exists(ico))
            ico = Join(g_dir, "assets\\brand\\dogecoin-testnet.ico");
        if (Exists(ico)) {
            HICON hi = (HICON)LoadImageA(nullptr, ico.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
            if (hi)
                wc.hIcon = hi;
        }
    }
    RegisterClassA(&wc);

    RECT wa;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.left + ((wa.right - wa.left) - kW) / 2;
    int y = wa.top + ((wa.bottom - wa.top) - kH) / 2;
    HWND h = CreateWindowExA(WS_EX_TOPMOST | WS_EX_APPWINDOW, wc.lpszClassName,
                             g_testnet ? "Dogecoin Core Pro  -  TESTNET"
                                       : "Dogecoin Core Pro  -  Hybrid",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             x, y, kW, kH, nullptr, nullptr, wc.hInstance, nullptr);
    if (!h)
        return 0; // default GUI
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    MSG msg;
    int cancelled = 0;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) {
            cancelled = (int)msg.wParam;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (cancelled == 1)
        return -1;
    return g_wantTui ? 1 : 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    g_dir = ExeDir();
    ParseCmdLine(lpCmdLine);
    const std::string role = Role();

    StartNodeIfNeeded();

    if (role == "server") {
        if (!StartTui())
            MessageBoxA(nullptr, "Operator TUI is missing from this install.", "Dogecoin Core Pro", MB_ICONERROR);
        return 0;
    }
    if (role == "client") {
        if (!StartGui())
            MessageBoxA(nullptr, "Desktop GUI is missing from this install.", "Dogecoin Core Pro", MB_ICONERROR);
        return 0;
    }

    std::string pref = HybridPref();
    if (pref == "ask") {
        int r = AskHybrid();
        if (r < 0)
            return 0;
        pref = r ? "tui" : "gfx";
        if (g_remember)
            WriteHybridPref(pref.c_str());
    }
    if (pref == "tui") {
        if (!StartTui())
            MessageBoxA(nullptr, "Operator TUI is missing from this install.", "Dogecoin Core Pro", MB_ICONERROR);
        return 0;
    }
    if (!StartGui())
        MessageBoxA(nullptr, "Desktop GUI is missing from this install.", "Dogecoin Core Pro", MB_ICONERROR);
    return 0;
}
