#include "tor_local.h"

#include <chrono>
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
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static long long NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static bool FileExists(const std::string& p)
{
    if (p.empty())
        return false;
#if defined(_WIN32)
    // GetFileAttributes — never open the PE (Defender scans ifstream/open every frame).
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(p.c_str(), F_OK) == 0;
#endif
}

static std::string g_torExeCache;
static long long g_torExeAt = 0;
static bool g_socksCache = false;
static int g_socksPortCache = -1;
static long long g_socksAt = 0;

void InvalidateTorStatusCache()
{
    g_torExeAt = 0;
    g_socksAt = 0;
    g_torExeCache.clear();
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
    const long long now = NowMs();
    if (g_torExeAt && (now - g_torExeAt) < 2000)
        return g_torExeCache;

    std::string found;
#if defined(_WIN32)
    const std::string official = ExpectedTorExe();
    if (FileExists(official))
        found = official;
    if (found.empty()) {
        const std::string nextToGuiFlat = ExeDir() + "\\tor.exe";
        if (FileExists(nextToGuiFlat))
            found = nextToGuiFlat;
    }
    if (found.empty()) {
        const char* cands[] = {
            "C:\\Program Files\\Dogecoin\\tor\\tor.exe",
            "C:\\dogedev\\tor\\tor.exe",
        };
        for (const char* c : cands) {
            if (c && FileExists(c)) {
                found = c;
                break;
            }
        }
    }
#else
    const char* cands[] = { "/usr/bin/tor", "/usr/sbin/tor", "/usr/local/bin/tor" };
    for (const char* c : cands) {
        if (FileExists(c)) {
            found = c;
            break;
        }
    }
#endif
    g_torExeCache = found;
    g_torExeAt = now;
    return g_torExeCache;
}

static bool ProbeSocksNow(int socksPort)
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
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)socksPort);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    int r = connect(s, (sockaddr*)&a, sizeof(a));
    bool ok = (r == 0);
    if (!ok) {
        const int e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK || e == WSAEINPROGRESS) {
            fd_set w;
            FD_ZERO(&w);
            FD_SET(s, &w);
            timeval tv {};
            tv.tv_usec = 2000; // 2ms — never stall ImGui
            if (select(0, nullptr, &w, nullptr, &tv) > 0) {
                int err = 0;
                int len = sizeof(err);
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
                ok = (err == 0);
            }
        }
    }
    closesocket(s);
    return ok;
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return false;
    int flags = fcntl(s, F_GETFL, 0);
    if (flags >= 0)
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)socksPort);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    int r = connect(s, (sockaddr*)&a, sizeof(a));
    bool ok = (r == 0);
    if (!ok && (errno == EINPROGRESS || errno == EWOULDBLOCK)) {
        fd_set w;
        FD_ZERO(&w);
        FD_SET(s, &w);
        timeval tv {};
        tv.tv_usec = 2000;
        if (select(s + 1, nullptr, &w, nullptr, &tv) > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
            ok = (err == 0);
        }
    }
    close(s);
    return ok;
#endif
}

bool TorSocksListening(int socksPort)
{
    const long long now = NowMs();
    if (g_socksAt && g_socksPortCache == socksPort && (now - g_socksAt) < 1000)
        return g_socksCache;
    g_socksCache = ProbeSocksNow(socksPort);
    g_socksPortCache = socksPort;
    g_socksAt = now;
    return g_socksCache;
}

bool StartLocalTor(const std::string& torExe, const std::string& dataDir, std::string& errOut)
{
    errOut.clear();
    InvalidateTorStatusCache();
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
