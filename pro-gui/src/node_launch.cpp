#include "node_launch.h"
#include "rpc_client.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsvc.h>
#include <tlhelp32.h>
#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif
#else
#include <cstdlib>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#endif

static bool FileExists(const std::string& p)
{
    std::ifstream f(p.c_str(), std::ios::binary);
    return (bool)f;
}

static std::string Dirname(std::string p)
{
    while (!p.empty() && (p.back() == '/' || p.back() == '\\'))
        p.pop_back();
    auto slash = p.find_last_of("/\\");
    if (slash == std::string::npos)
        return ".";
    return p.substr(0, slash);
}

std::string MakeAbsolutePath(const std::string& path)
{
    if (path.empty())
        return {};
#if defined(_WIN32)
    char out[MAX_PATH];
    DWORD n = GetFullPathNameA(path.c_str(), MAX_PATH, out, nullptr);
    if (n == 0 || n >= MAX_PATH)
        return path;
    return std::string(out);
#else
    char* r = realpath(path.c_str(), nullptr);
    if (!r)
        return path;
    std::string s(r);
    free(r);
    return s;
#endif
}

static std::string ToLowerCopy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static bool PathHasFile(const std::string& dir, const char* name)
{
    if (dir.empty() || !name)
        return false;
    return FileExists(dir + "\\" + name) || FileExists(dir + "/" + name);
}

static bool DirLooksLikeCoreProInstall(const std::string& dir)
{
    if (dir.empty())
        return false;
    return PathHasFile(dir, "corepro-launch.exe") || PathHasFile(dir, "dogecoin-pro-gui.exe") ||
           PathHasFile(dir, "gpenode-ops.exe") || PathHasFile(dir, "dogecoin-pro-gui-smoke.exe") ||
           PathHasFile(dir, "corepro-launch") || PathHasFile(dir, "dogecoin-pro-gui") ||
           PathHasFile(dir, "gpenode-ops");
}

static bool DirLooksLikeOfficialCoreOnly(const std::string& dir)
{
    if (dir.empty() || DirLooksLikeCoreProInstall(dir))
        return false;
    return PathHasFile(dir, "dogecoin-qt.exe") || PathHasFile(dir, "dogecoin-qt");
}

#if defined(_WIN32)
static std::string ThisExeDir()
{
    char buf[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, buf, MAX_PATH))
        return {};
    return Dirname(MakeAbsolutePath(buf));
}

static std::string OurInstallRoot()
{
    std::string d = ThisExeDir();
    if (d.empty())
        return {};
    std::string leaf = d;
    auto slash = leaf.find_last_of("/\\");
    if (slash != std::string::npos)
        leaf = leaf.substr(slash + 1);
    if (ToLowerCopy(leaf) == "daemon" || ToLowerCopy(leaf) == "bin")
        return Dirname(d);
    return d;
}

static bool PathIsUnder(const std::string& file, const std::string& root)
{
    if (file.empty() || root.empty())
        return false;
    std::string f = ToLowerCopy(MakeAbsolutePath(file));
    std::string r = ToLowerCopy(MakeAbsolutePath(root));
    while (!r.empty() && (r.back() == '\\' || r.back() == '/'))
        r.pop_back();
    if (f.size() < r.size())
        return false;
    if (f.compare(0, r.size(), r) != 0)
        return false;
    return f.size() == r.size() || f[r.size()] == '\\' || f[r.size()] == '/';
}

static bool QueryProcessImagePath(DWORD pid, std::string& out)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return false;
    char buf[MAX_PATH];
    DWORD n = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameA(h, 0, buf, &n);
    CloseHandle(h);
    if (!ok || !buf[0])
        return false;
    out = MakeAbsolutePath(buf);
    return true;
}

struct ProcHit {
    std::string exeName;
    std::string imagePath;
    DWORD pid = 0;
};

static std::vector<ProcHit> SnapshotNamed(const char* exeA)
{
    std::vector<ProcHit> hits;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return hits;
    PROCESSENTRY32 pe {};
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, exeA) != 0)
                continue;
            ProcHit h;
            h.exeName = pe.szExeFile;
            h.pid = pe.th32ProcessID;
            QueryProcessImagePath(pe.th32ProcessID, h.imagePath);
            hits.push_back(h);
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return hits;
}
#else
static std::string ThisExeDir()
{
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = 0;
    return Dirname(MakeAbsolutePath(buf));
}

static std::string OurInstallRoot()
{
    std::string d = ThisExeDir();
    if (d.empty())
        return {};
    std::string leaf = d;
    auto slash = leaf.find_last_of('/');
    if (slash != std::string::npos)
        leaf = leaf.substr(slash + 1);
    if (leaf == "daemon" || leaf == "bin")
        return Dirname(d);
    if (d == "/usr/bin" || d == "/usr/local/bin")
        return "/usr/lib/dogecoin-core-pro";
    return d;
}

static bool PathIsUnder(const std::string& file, const std::string& root)
{
    if (file.empty() || root.empty())
        return false;
    std::string f = MakeAbsolutePath(file);
    std::string r = MakeAbsolutePath(root);
    while (!r.empty() && r.back() == '/')
        r.pop_back();
    if (f.size() < r.size())
        return false;
    if (f.compare(0, r.size(), r) != 0)
        return false;
    return f.size() == r.size() || f[r.size()] == '/';
}

struct ProcHit {
    std::string exeName;
    std::string imagePath;
    unsigned long pid = 0;
};

static std::string ProcComm(const char* pid)
{
    std::string p = std::string("/proc/") + pid + "/comm";
    std::ifstream f(p.c_str());
    std::string s;
    std::getline(f, s);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    return s;
}

static std::string ProcExe(const char* pid)
{
    char buf[PATH_MAX];
    std::string p = std::string("/proc/") + pid + "/exe";
    ssize_t n = readlink(p.c_str(), buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = 0;
    return MakeAbsolutePath(buf);
}

static std::vector<ProcHit> SnapshotNamed(const char* comm)
{
    std::vector<ProcHit> hits;
    DIR* d = opendir("/proc");
    if (!d)
        return hits;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] < '1' || e->d_name[0] > '9')
            continue;
        if (ProcComm(e->d_name) != comm)
            continue;
        ProcHit h;
        h.exeName = comm;
        h.pid = std::strtoul(e->d_name, nullptr, 10);
        h.imagePath = ProcExe(e->d_name);
        hits.push_back(h);
    }
    closedir(d);
    return hits;
}
#endif

std::vector<std::string> FindDogecoindCandidates(const std::string& assetsRoot)
{
    std::vector<std::string> out;
    std::string base = Dirname(assetsRoot);
    auto add = [&](const std::string& p) {
        if (p.empty())
            return;
        std::string abs = MakeAbsolutePath(p);
        if (!FileExists(abs) && FileExists(p))
            abs = MakeAbsolutePath(p);
        if (!FileExists(abs))
            return;
        const std::string dir = Dirname(abs);
        if (DirLooksLikeOfficialCoreOnly(dir) || DirLooksLikeOfficialCoreOnly(Dirname(dir)))
            return;
        out.push_back(abs);
    };

#if defined(_WIN32)
    add(base + "\\dogecoind.exe");
    add(base + "\\..\\dogecoind.exe");
    add(base + "\\..\\..\\dogecoind.exe");
    add(base + "\\..\\..\\release\\dogecoind.exe");
    add("C:\\dogedev\\release\\dogecoind.exe");
    add("C:\\dogedev\\release\\smoke-pro-gui\\dogecoind.exe");
    add("C:\\Program Files\\Dogecoin\\daemon\\dogecoind.exe");
    add("C:\\Program Files\\Dogecoin\\dogecoind.exe");
    char buf[MAX_PATH];
    if (GetModuleFileNameA(nullptr, buf, MAX_PATH)) {
        std::string exeDir = Dirname(buf);
        add(exeDir + "\\dogecoind.exe");
        add(exeDir + "\\..\\dogecoind.exe");
        add(exeDir + "\\daemon\\dogecoind.exe");
    }
#else
    add(base + "/dogecoind");
    add(base + "/../dogecoind");
    add(base + "/../../src/dogecoind");
    add("/usr/lib/dogecoin-core-pro/dogecoind");
    add("/usr/local/bin/dogecoind");
    add("/usr/bin/dogecoind");
#endif
    std::vector<std::string> uniq;
    for (const auto& p : out) {
        bool seen = false;
        for (const auto& u : uniq)
            if (u == p) {
                seen = true;
                break;
            }
        if (!seen)
            uniq.push_back(p);
    }
    return uniq;
}

std::string FindBestDogecoind(const std::string& assetsRoot)
{
    auto c = FindDogecoindCandidates(assetsRoot);
    if (c.empty())
        return {};
    return c.front();
}

std::string FindProGuiExe()
{
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, buf, MAX_PATH))
        return {};
    std::string self = MakeAbsolutePath(buf);
    std::string dir = Dirname(self);
    const char* names[] = {
        "dogecoin-pro-gui-smoke.exe",
        "dogecoin-pro-gui.exe",
    };
    for (const char* n : names) {
        std::string p = dir + "\\" + n;
        if (!FileExists(p))
            continue;
        // Never return dogeinit itself
        if (MakeAbsolutePath(p) == self)
            continue;
        return MakeAbsolutePath(p);
    }
    return {};
#else
    return {};
#endif
}

bool IsDogecoindRunning()
{
    static bool last = false;
    static long long at = 0;
    const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now().time_since_epoch())
                              .count();
    if (at && (now - at) < 400)
        return last;
    at = now;
#if defined(_WIN32)
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        last = false;
        return false;
    }
    PROCESSENTRY32 pe {};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, "dogecoind.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    last = found;
#else
    last = system("pgrep -x dogecoind >/dev/null 2>&1") == 0;
#endif
    return last;
}

#if defined(_WIN32)
static const char* kDogeProc = "dogecoind.exe";
static const char* kQtProc = "dogecoin-qt.exe";
#else
static const char* kDogeProc = "dogecoind";
static const char* kQtProc = "dogecoin-qt";
#endif

static bool ImageIsOurDogecoind(const std::string& imagePath)
{
    if (imagePath.empty())
        return false;
    const std::string root = OurInstallRoot();
    if (PathIsUnder(imagePath, root) || DirLooksLikeCoreProInstall(Dirname(imagePath)))
        return true;
    if (imagePath.find("dogecoin-core-pro") != std::string::npos)
        return true;
    return FileExists("/usr/lib/dogecoin-core-pro/dogecoind") &&
           (imagePath == "/usr/bin/dogecoind" || imagePath == "/usr/lib/dogecoin-core-pro/dogecoind");
}

bool OurDogecoindRunning()
{
    if (QueryCoreProService() == CoreProServiceState::Running)
        return true;
    for (const auto& h : SnapshotNamed(kDogeProc)) {
        if (ImageIsOurDogecoind(h.imagePath))
            return true;
    }
    return false;
}

bool ForeignDogecoinNodeRunning(std::string& detailOut)
{
    detailOut.clear();
    for (const auto& h : SnapshotNamed(kQtProc)) {
        detailOut = "Official Dogecoin Core (dogecoin-qt) is running";
        if (!h.imagePath.empty())
            detailOut += ":\n" + h.imagePath;
        detailOut += "\nClose it before starting Core Pro. They cannot share a datadir.";
        return true;
    }
    for (const auto& h : SnapshotNamed(kDogeProc)) {
        if (ImageIsOurDogecoind(h.imagePath))
            continue;
        if (QueryCoreProService() == CoreProServiceState::Running && h.imagePath.empty())
            continue;
        if (!h.imagePath.empty() &&
            (DirLooksLikeOfficialCoreOnly(Dirname(h.imagePath)) ||
             DirLooksLikeOfficialCoreOnly(Dirname(Dirname(h.imagePath))))) {
            detailOut = "Official dogecoind is running:\n" + h.imagePath +
                        "\nClose Dogecoin Core before starting Core Pro.";
            return true;
        }
        if (h.imagePath.empty()) {
            detailOut = "A dogecoind process is running that is not this Core Pro install. "
                        "Close official Dogecoin Core (and any other node) first.";
            return true;
        }
        detailOut = "Another dogecoind is running:\n" + h.imagePath +
                    "\nCore Pro will not attach to it. Close that process first.";
        return true;
    }
    return false;
}

bool StartDogecoind(const std::string& exePath, const std::string& datadirHint, std::string& errOut,
                    int pruneMiB, int dbCacheMb, const std::string& archivePath,
                    const std::string& dbEngine, const std::string& extraArgs)
{
    errOut.clear();
    std::string abs = MakeAbsolutePath(exePath);
    if (abs.empty() || !FileExists(abs)) {
        errOut = "dogecoind not found";
        return false;
    }

    if (IsDogecoindRunning()) {
        std::string foreign;
        if (ForeignDogecoinNodeRunning(foreign)) {
            errOut = foreign.empty() ? "another Dogecoin Core is running" : foreign;
            return false;
        }
        errOut = "dogecoind already running";
        return true; // our node — RPC should come up
    }

    // CRITICAL: pruned datadirs abort AppInit without -prune=
    // Mom/Fast Sync installs use prune; launching bare dogecoind exits after LoadBlockIndex
    // ("This datadir was previously pruned") → RPC port opens then dies → splash Step 2 forever.
#if defined(_WIN32)
    std::string cmd = "\"" + abs + "\" -server";
    if (pruneMiB > 0)
        cmd += " -prune=" + std::to_string(pruneMiB);
    if (dbCacheMb > 0)
        cmd += " -dbcache=" + std::to_string(dbCacheMb);
    if (!datadirHint.empty())
        cmd += " -datadir=\"" + datadirHint + "\"";
    if (!archivePath.empty())
        cmd += " -archivepath=\"" + archivePath + "\"";
    if (dbEngine == "mdbx" || dbEngine == "leveldb")
        cmd += " -dbengine=" + dbEngine;
    if (!extraArgs.empty()) {
        cmd += " ";
        cmd += extraArgs;
    }

    // Working directory = dogecoind's folder (helps relative conf / DLLs)
    std::string workDir = Dirname(abs);

    STARTUPINFOA si {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi {};
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');

    BOOL ok = CreateProcessA(
        abs.c_str(),
        cmdline.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW | DETACHED_PROCESS,
        nullptr,
        workDir.c_str(),
        &si,
        &pi);
    if (!ok) {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "CreateProcess failed (err=%lu)", GetLastError());
        errOut = msg;
        return false;
    }
    // Keep process alive independently; close handles
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Brief settle — if it dies immediately, caller will see port never stay up
    Sleep(500);
    if (!IsDogecoindRunning()) {
        errOut = "dogecoind exited immediately — check %APPDATA%\\Dogecoin\\debug.log "
                 "(often: pruned datadir needs -prune=5500; we pass that now)";
        return false;
    }
    return true;
#else
    std::string cmd = "\"" + abs + "\" -daemon -server";
    if (pruneMiB > 0)
        cmd += " -prune=" + std::to_string(pruneMiB);
    if (dbCacheMb > 0)
        cmd += " -dbcache=" + std::to_string(dbCacheMb);
    if (!datadirHint.empty())
        cmd += " -datadir=\"" + datadirHint + "\"";
    if (!archivePath.empty())
        cmd += " -archivepath=\"" + archivePath + "\"";
    if (dbEngine == "mdbx" || dbEngine == "leveldb")
        cmd += " -dbengine=" + dbEngine;
    if (!extraArgs.empty()) {
        cmd += " ";
        cmd += extraArgs;
    }
    cmd += " >/tmp/dogecoind-pro-gui.log 2>&1";
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        errOut = "failed to spawn dogecoind (see /tmp/dogecoind-pro-gui.log)";
        return false;
    }
    return true;
#endif
}

std::string DefaultDebugLogPath(const std::string& network)
{
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (!appdata || !appdata[0])
        return {};
    if (network == "test")
        return std::string(appdata) + "\\Dogecoin\\testnet3\\debug.log";
    if (network == "regtest")
        return std::string(appdata) + "\\Dogecoin\\regtest\\debug.log";
    return std::string(appdata) + "\\Dogecoin\\debug.log";
#else
    const char* home = std::getenv("HOME");
    if (!home) return {};
    if (network == "test")
        return std::string(home) + "/.dogecoin/testnet3/debug.log";
    if (network == "regtest")
        return std::string(home) + "/.dogecoin/regtest/debug.log";
    return std::string(home) + "/.dogecoin/debug.log";
#endif
}

std::string TailInitMessageFromDebugLog(const std::string& logPath)
{
    if (logPath.empty() || !FileExists(logPath))
        return {};
    std::ifstream f(logPath.c_str(), std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    if (sz <= 0) return {};
    const std::streamoff maxTail = 48 * 1024;
    std::streamoff start = (sz > maxTail) ? (std::streamoff)sz - maxTail : 0;
    f.seekg(start);
    std::string tail((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Prefer the most recent known InitMessage (same strings core emits to splash)
    static const char* keys[] = {
        "Error opening block database",
        "Aborted block database rebuild",
        "MDBX ",
        "Done loading",
        "Starting network threads",
        "Loading banlist",
        "Loading addresses",
        "Rescanning",
        "Loading wallet",
        "Verifying wallet",
        "Pruning blockstore",
        "Verifying blocks",
        "Rewinding blocks",
        "Loading block index",
        "init message:",
        "Initializing",
    };
    std::string best;
    size_t bestPos = std::string::npos;
    for (const char* k : keys) {
        auto p = tail.rfind(k);
        if (p == std::string::npos) continue;
        if (bestPos == std::string::npos || p > bestPos) {
            bestPos = p;
            // Take rest of line
            auto e = tail.find('\n', p);
            std::string line = (e == std::string::npos) ? tail.substr(p) : tail.substr(p, e - p);
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();
            best = line;
        }
    }
    return best;
}

#if defined(_WIN32)
static const char* kCoreProService = "DogecoinGPENode";

CoreProServiceState QueryCoreProService()
{
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return CoreProServiceState::Missing;
    SC_HANDLE svc = OpenServiceA(scm, kCoreProService, SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return CoreProServiceState::Missing;
    }
    SERVICE_STATUS_PROCESS ssp {};
    DWORD need = 0;
    CoreProServiceState st = CoreProServiceState::Other;
    if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &need)) {
        if (ssp.dwCurrentState == SERVICE_RUNNING)
            st = CoreProServiceState::Running;
        else if (ssp.dwCurrentState == SERVICE_STOPPED)
            st = CoreProServiceState::Stopped;
        else
            st = CoreProServiceState::Other;
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return st;
}

bool StartCoreProService(std::string& errOut)
{
    errOut.clear();
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        errOut = "Cannot open Service Control Manager";
        return false;
    }
    SC_HANDLE svc = OpenServiceA(scm, kCoreProService, SERVICE_START | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        errOut = "DogecoinGPENode service is not installed or not controllable";
        return false;
    }
    BOOL started = StartServiceA(svc, 0, nullptr);
    const DWORD err = GetLastError();
    if (!started && err != ERROR_SERVICE_ALREADY_RUNNING) {
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        errOut = "Could not start DogecoinGPENode (Win32 " + std::to_string(err) + ")";
        return false;
    }
    for (int i = 0; i < 50; ++i) {
        SERVICE_STATUS_PROCESS ssp {};
        DWORD need = 0;
        if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &need)) {
            if (ssp.dwCurrentState == SERVICE_RUNNING) {
                CloseServiceHandle(svc);
                CloseServiceHandle(scm);
                return true;
            }
            if (ssp.dwCurrentState == SERVICE_STOPPED && i > 2)
                break;
        }
        Sleep(200);
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return IsDogecoindRunning();
}

bool StopCoreProServiceWait(int waitMs, std::string& errOut)
{
    errOut.clear();
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        errOut = "Cannot open Service Control Manager";
        return false;
    }
    SC_HANDLE svc = OpenServiceA(scm, kCoreProService, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return true; // no service to stop
    }
    SERVICE_STATUS st {};
    ControlService(svc, SERVICE_CONTROL_STOP, &st);
    const DWORD t0 = GetTickCount();
    bool stopped = false;
    while ((int)(GetTickCount() - t0) < waitMs) {
        SERVICE_STATUS_PROCESS ssp {};
        DWORD need = 0;
        if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &need) &&
            ssp.dwCurrentState == SERVICE_STOPPED) {
            stopped = true;
            break;
        }
        Sleep(250);
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (!stopped)
        errOut = "Service did not report STOPPED";
    return stopped;
}

bool RestartCoreProNode(const std::string& host, int port,
                        const std::string& cookiePath,
                        const std::string& user, const std::string& pass,
                        std::string& errOut)
{
    errOut.clear();
    std::string stopErr;
    RequestDogecoindStop(host, port, cookiePath, user, pass, stopErr);
    const DWORD t0 = GetTickCount();
    while (IsDogecoindRunning() && (GetTickCount() - t0) < 180000)
        Sleep(500);
    if (IsDogecoindRunning()) {
        errOut = "dogecoind is still flushing — not killed. Wait, then retry.";
        return false;
    }
    StopCoreProServiceWait(15000, stopErr);
    if (QueryCoreProService() == CoreProServiceState::Missing) {
        errOut = "Service is not installed";
        return false;
    }
    return StartCoreProService(errOut);
}
#else
static bool SystemctlQuiet(const std::string& args)
{
    std::string cmd = "systemctl " + args + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

CoreProServiceState QueryCoreProService()
{
    if (!SystemctlQuiet("cat dogecoin-core-pro.service") &&
        !SystemctlQuiet("cat dogecoin-gpenode.service"))
        return CoreProServiceState::Missing;
    if (SystemctlQuiet("is-active dogecoin-core-pro") ||
        SystemctlQuiet("is-active dogecoin-gpenode"))
        return CoreProServiceState::Running;
    return CoreProServiceState::Stopped;
}

bool StartCoreProService(std::string& errOut)
{
    errOut.clear();
    if (QueryCoreProService() == CoreProServiceState::Missing) {
        errOut = "systemd unit dogecoin-core-pro is not installed";
        return false;
    }
    if (SystemctlQuiet("start dogecoin-core-pro") || SystemctlQuiet("start dogecoin-gpenode")) {
        for (int i = 0; i < 40; ++i) {
            if (QueryCoreProService() == CoreProServiceState::Running)
                return true;
            usleep(200 * 1000);
        }
        return IsDogecoindRunning();
    }
    errOut = "Could not start dogecoin-core-pro (try: sudo systemctl start dogecoin-core-pro)";
    return false;
}

bool StopCoreProServiceWait(int waitMs, std::string& errOut)
{
    errOut.clear();
    if (QueryCoreProService() == CoreProServiceState::Missing)
        return true;
    (void)SystemctlQuiet("stop dogecoin-core-pro");
    (void)SystemctlQuiet("stop dogecoin-gpenode");
    const int steps = std::max(1, waitMs / 250);
    for (int i = 0; i < steps; ++i) {
        if (QueryCoreProService() != CoreProServiceState::Running)
            return true;
        usleep(250 * 1000);
    }
    errOut = "systemd unit did not stop (Restart=on-failure may need sudo systemctl stop)";
    return false;
}
#endif

bool RequestDogecoindStop(const std::string& host, int port,
                          const std::string& cookiePath,
                          const std::string& user, const std::string& pass,
                          std::string& errOut)
{
    errOut.clear();
    if (!IsDogecoindRunning())
        return true;

    RpcConfig cfg;
    cfg.host = host.empty() ? "127.0.0.1" : host;
    cfg.port = port > 0 ? port : 22555;
    cfg.cookiePath = cookiePath;
    cfg.user = user;
    cfg.password = pass;
    RpcClient client(cfg);
    RpcResult r = client.call("stop");
    // stop often closes the socket before a clean JSON reply — treat transport drop as OK
    if (r.ok || r.httpCode == 200 || r.body.find("Dogecoin server stopping") != std::string::npos ||
        r.error.find("recv") != std::string::npos || r.error.find("connect") != std::string::npos ||
        r.error.find("closed") != std::string::npos || r.error.find("reset") != std::string::npos) {
        return true;
    }
    // Port may already be gone mid-shutdown
    if (!client.portOpen(200))
        return true;
    errOut = r.error.empty() ? "stop RPC failed" : r.error;
    return false;
}

bool StartProGui(const std::string& guiPath, std::string& errOut)
{
    errOut.clear();
    std::string abs = MakeAbsolutePath(guiPath);
    if (abs.empty() || !FileExists(abs)) {
        errOut = "GUI exe not found";
        return false;
    }
#if defined(_WIN32)
    std::string workDir = Dirname(abs);
    STARTUPINFOA si {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi {};
    std::string cmd = "\"" + abs + "\"";
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');
    BOOL ok = CreateProcessA(
        abs.c_str(),
        cmdline.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workDir.c_str(),
        &si,
        &pi);
    if (!ok) {
        errOut = "failed to start GUI";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    std::string cmd = "\"" + abs + "\" &";
    return std::system(cmd.c_str()) == 0;
#endif
}
