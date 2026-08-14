#include "tor_local.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static bool FileExists(const std::string& p)
{
    std::ifstream f(p.c_str());
    return (bool)f;
}

static std::string ExeDir()
{
#if defined(_WIN32)
    char self[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string exeDir = self;
    auto sl = exeDir.find_last_of("\\/");
    if (sl != std::string::npos)
        exeDir.resize(sl);
    return exeDir;
#else
    return {};
#endif
}

std::string ExpectedTorDir()
{
#if defined(_WIN32)
    return ExeDir() + "\\tor";
#else
    return {};
#endif
}

std::string ExpectedTorExe()
{
    const std::string d = ExpectedTorDir();
    if (d.empty())
        return {};
    return d + "\\tor.exe";
}

std::string FindTorExecutable()
{
#if defined(_WIN32)
    const std::string official = ExpectedTorExe();
    if (FileExists(official))
        return official;
    const std::string nextToGuiFlat = ExeDir() + "\\tor.exe";
    if (FileExists(nextToGuiFlat))
        return nextToGuiFlat;
    const char* cands[] = {
        "C:\\Program Files\\Dogecoin\\tor\\tor.exe",
        "C:\\dogedev\\tor\\tor.exe",
    };
    for (const char* c : cands) {
        if (c && FileExists(c))
            return c;
    }
#else
    const char* cands[] = { "/usr/bin/tor", "/usr/sbin/tor", "/usr/local/bin/tor" };
    for (const char* c : cands) {
        if (FileExists(c))
            return c;
    }
#endif
    return {};
}

bool TorSocksListening(int socksPort)
{
#if defined(_WIN32)
    static bool wsa = false;
    if (!wsa) {
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d) != 0)
            return false;
        wsa = true;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
        return false;
    DWORD ms = 400;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof(ms));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&ms, sizeof(ms));
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)socksPort);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    const int ok = connect(s, (sockaddr*)&a, sizeof(a));
    closesocket(s);
    return ok == 0;
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return false;
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)socksPort);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    const int ok = connect(s, (sockaddr*)&a, sizeof(a));
    close(s);
    return ok == 0;
#endif
}

bool StartLocalTor(const std::string& torExe, const std::string& dataDir, std::string& errOut)
{
    errOut.clear();
    if (TorSocksListening(9050))
        return true;
    if (torExe.empty() || !FileExists(torExe)) {
        errOut = "tor.exe not found. Put the Tor Expert Bundle in Dogecoin\\tor\\ (next to the GUI).";
        return false;
    }
    if (dataDir.empty()) {
        errOut = "No Tor data directory";
        return false;
    }
#if defined(_WIN32)
    CreateDirectoryA(dataDir.c_str(), nullptr);
    const std::string rc = dataDir + "\\torrc";
    {
        std::ofstream f(rc.c_str());
        if (!f) {
            errOut = "Could not write torrc";
            return false;
        }
        f << "# Core Pro Phase 1 — SOCKS for dogecoind P2P only.\n";
        f << "# No ControlPort: inbound onion is Advanced (not this toggle).\n";
        f << "DataDirectory " << dataDir << "\n";
        f << "SocksPort 127.0.0.1:9050\n";
        f << "SocksPolicy accept 127.0.0.1\n";
        f << "SocksPolicy reject *\n";
        f << "AvoidDiskWrites 0\n";
        f << "SafeLogging 1\n";
    }

    std::string cmd = "\"" + torExe + "\" -f \"" + rc + "\"";
    STARTUPINFOA si {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi {};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    BOOL ok = CreateProcessA(torExe.c_str(), buf.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                             dataDir.c_str(), &si, &pi);
    if (!ok) {
        errOut = "CreateProcess tor.exe failed (Win32 " + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    for (int i = 0; i < 40; ++i) {
        Sleep(250);
        if (TorSocksListening(9050))
            return true;
    }
    errOut = "tor.exe started but SOCKS 127.0.0.1:9050 did not open";
    return false;
#else
    (void)torExe;
    (void)dataDir;
    errOut = "Start Tor from the GUI is Windows-only in Phase 1";
    return TorSocksListening(9050);
#endif
}
