#include "app.h"

#include "arcade_host.h"
#include "node_launch.h"
#include "cloudpath.h"
#include "qr_tiny.h"
#include "tor_local.h"

#include "imgui.h"
// IsDogecoindRunning used in TickNodeProbe
#include "imgui_internal.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
void ProGuiSetCaptionDragRect(int x0, int y0, int x1, int y1);
void ProGuiSetCaptionButtonLeft(int x);
void ProGuiSetCaptionMenuRight(int x);
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <winhttp.h>
#include "win_tray.h"
#else
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include "linux_tray.h"
#endif

static bool DirectoryExists(const char* path)
{
    if (!path || !path[0])
        return false;
#if defined(_WIN32)
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return ::stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static bool FileExists(const char* path)
{
    if (!path || !path[0])
        return false;
#if defined(_WIN32)
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return ::stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

/** IBD can stall getblockchaininfo; never paint the -1 sentinel. */
static int DisplayBlocks(const NodeSnapshot& s, int fallback = 0)
{
    if (s.blocks >= 0)
        return s.blocks;
    return fallback > 0 ? fallback : -1;
}

static int DisplayHeaders(const NodeSnapshot& s, int fallback = 0)
{
    if (s.headers >= 0)
        return s.headers;
    return fallback > 0 ? fallback : -1;
}

static void DrawDbcacheGauge(const NodeSnapshot& s)
{
    if (s.dbcacheLimitBytes <= 0)
        return;
    const double used = (double)s.dbcacheBytes;
    const double lim = (double)s.dbcacheLimitBytes;
    float frac = (float)(used / lim);
    if (frac < 0.f)
        frac = 0.f;
    if (frac > 1.f)
        frac = 1.f;
    char ov[72];
    std::snprintf(ov, sizeof(ov), "%.1f / %.1f GiB  (%.0f%%)",
                  used / (1024.0 * 1024.0 * 1024.0),
                  lim / (1024.0 * 1024.0 * 1024.0),
                  frac * 100.0);
    ImGui::TextUnformatted("UTXO cache");
    ImGui::ProgressBar(frac, ImVec2(-1, 0), ov);
    if (frac >= 0.92f)
        ImGui::TextDisabled("Cache near full — a disk flush can pause IBD for minutes. Not a freeze.");
}

static void DrawBlocksHeadersLine(const char* prefix, int blocks, int headers)
{
    if (blocks < 0 && headers <= 0)
        ImGui::Text("%s —", prefix);
    else if (headers > 0 && blocks >= 0)
        ImGui::Text("%s %d  /  %d  headers", prefix, blocks, headers);
    else if (blocks >= 0)
        ImGui::Text("%s %d", prefix, blocks);
    else
        ImGui::Text("%s —  /  %d  headers", prefix, headers);
}

/** Keep last height/wallet/peers when the probe times out but dogecoind is still up. */
static void HoldLastGoodOnProbeMiss(NodeSnapshot& incoming, const NodeSnapshot& previous)
{
    if (incoming.connected || previous.blocks < 0)
        return;
    if (!IsDogecoindRunning())
        return;
    incoming.stale = true;
    incoming.connected = true;
    incoming.rpcWarmup = previous.rpcWarmup;
    incoming.blocks = previous.blocks;
    incoming.headers = previous.headers;
    incoming.verificationProgress = previous.verificationProgress;
    incoming.initialBlockDownload = previous.initialBlockDownload;
    incoming.bestBlockHash = previous.bestBlockHash;
    incoming.dbEngine = previous.dbEngine;
    incoming.chain = previous.chain;
    incoming.hasWallet = previous.hasWallet;
    incoming.balance = previous.balance;
    incoming.unconfirmed = previous.unconfirmed;
    incoming.walletEncrypted = previous.walletEncrypted;
    incoming.walletLocked = previous.walletLocked;
    incoming.walletName = previous.walletName;
    incoming.hasIbdInfo = previous.hasIbdInfo;
    incoming.ibdSummary = previous.ibdSummary;
    incoming.dbcacheBytes = previous.dbcacheBytes;
    incoming.dbcacheLimitBytes = previous.dbcacheLimitBytes;
    incoming.assumeUtxoValidated = previous.assumeUtxoValidated;
    incoming.assumeUtxoFailed = previous.assumeUtxoFailed;
    incoming.assumeUtxoDualCollapsed = previous.assumeUtxoDualCollapsed;
    incoming.assumeUtxoProgress = previous.assumeUtxoProgress;
    incoming.snapshotChainstateActive = previous.snapshotChainstateActive;
    incoming.hasMining = previous.hasMining;
    incoming.miningBlocks = previous.miningBlocks;
    incoming.difficulty = previous.difficulty;
    incoming.networkHashPs = previous.networkHashPs;
    incoming.pooledTx = previous.pooledTx;
    incoming.connections = previous.connections;
    incoming.peerCount = previous.peerCount;
    incoming.peers = previous.peers;
    incoming.peerLines = previous.peerLines;
    incoming.subversion = previous.subversion;
    incoming.version = previous.version;
    incoming.status = previous.status;
}

static bool WalletExistsIn(const char* datadir)
{
    if (!datadir || !datadir[0])
        return false;
    char w[560];
    std::snprintf(w, sizeof(w), "%s\\wallet.dat", datadir);
    if (FileExists(w))
        return true;
    std::snprintf(w, sizeof(w), "%s/wallet.dat", datadir);
    return FileExists(w);
}

static void FillDefaultDatadir(char* out, size_t n)
{
    if (!out || n == 0)
        return;
    out[0] = 0;
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0])
        std::snprintf(out, n, "%s\\Dogecoin", appdata);
#else
    const char* home = std::getenv("HOME");
    if (home && home[0])
        std::snprintf(out, n, "%s/.dogecoin", home);
#endif
}

static bool SamePath(const char* a, const char* b)
{
    if (!a || !b)
        return false;
#if defined(_WIN32)
    return _stricmp(a, b) == 0;
#else
    return std::strcmp(a, b) == 0;
#endif
}

static bool EnsureDirectory(const char* path)
{
    if (!path || !path[0])
        return false;
    if (DirectoryExists(path))
        return true;
#if defined(_WIN32)
    char buf[MAX_PATH];
    std::snprintf(buf, sizeof(buf), "%s", path);
    for (char* p = buf; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            if (buf[0] && !(buf[1] == ':' && buf[2] == 0))
                CreateDirectoryA(buf, nullptr);
            *p = c;
        }
    }
    return CreateDirectoryA(buf, nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS || DirectoryExists(buf);
#else
    return ::mkdir(path, 0755) == 0 || errno == EEXIST || DirectoryExists(path);
#endif
}

#if defined(_WIN32)
#include <shlobj.h>
static bool PickFolder(char* out, size_t outN)
{
    if (!out || outN == 0)
        return false;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IFileDialog, (void**)&dlg);
    if (FAILED(hr) || !dlg)
        return false;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(L"Choose data directory");
    hr = dlg->Show(nullptr);
    bool ok = false;
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR wpath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) && wpath) {
                WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out, (int)outN, nullptr, nullptr);
                CoTaskMemFree(wpath);
                ok = out[0] != 0;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

static bool PickOpenFile(char* out, size_t outN, const wchar_t* title)
{
    if (!out || outN == 0)
        return false;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IFileDialog, (void**)&dlg);
    if (FAILED(hr) || !dlg)
        return false;
    COMDLG_FILTERSPEC filters[] = {
        {L"Dogecoin wallet", L"wallet.dat"},
        {L"All files", L"*.*"},
    };
    dlg->SetFileTypes(2, filters);
    dlg->SetTitle(title ? title : L"Choose wallet.dat");
    hr = dlg->Show(nullptr);
    bool ok = false;
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR wpath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) && wpath) {
                WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out, (int)outN, nullptr, nullptr);
                CoTaskMemFree(wpath);
                ok = out[0] != 0;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

static bool PickSaveFile(char* out, size_t outN, const wchar_t* title)
{
    if (!out || outN == 0)
        return false;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IFileDialog, (void**)&dlg);
    if (FAILED(hr) || !dlg)
        return false;
    COMDLG_FILTERSPEC filters[] = {
        {L"Dogecoin wallet", L"wallet.dat"},
        {L"All files", L"*.*"},
    };
    dlg->SetFileTypes(2, filters);
    dlg->SetFileName(L"wallet.dat");
    dlg->SetTitle(title ? title : L"Backup wallet.dat");
    hr = dlg->Show(nullptr);
    bool ok = false;
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR wpath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) && wpath) {
                WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out, (int)outN, nullptr, nullptr);
                CoTaskMemFree(wpath);
                ok = out[0] != 0;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

static bool CopyFileUtf8(const char* src, const char* dst, std::string& err)
{
    if (!src || !src[0] || !dst || !dst[0]) {
        err = "Missing source or destination path.";
        return false;
    }
    if (!CopyFileA(src, dst, FALSE)) {
        char msg[160];
        std::snprintf(msg, sizeof(msg), "Copy failed (error %lu). Is the node still running?",
                      (unsigned long)GetLastError());
        err = msg;
        return false;
    }
    return true;
}
#endif

static std::string JsonEscape(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"')
            o.push_back('\\');
        o.push_back(c);
    }
    return o;
}

static std::string JsonField(const std::string& j, const char* key)
{
    std::string k = std::string("\"") + key + "\"";
    auto p = j.find(k);
    if (p == std::string::npos)
        return {};
    p = j.find(':', p);
    if (p == std::string::npos)
        return {};
    ++p;
    while (p < j.size() && std::isspace((unsigned char)j[p]))
        ++p;
    if (p >= j.size())
        return {};
    if (j[p] == '"') {
        auto e = j.find('"', p + 1);
        if (e == std::string::npos)
            return {};
        return j.substr(p + 1, e - p - 1);
    }
    auto e = p;
    while (e < j.size() && j[e] != ',' && j[e] != '}' && j[e] != '\n' && j[e] != '\r')
        ++e;
    std::string v = j.substr(p, e - p);
    while (!v.empty() && std::isspace((unsigned char)v.back()))
        v.pop_back();
    return v;
}

#if defined(_WIN32)
static std::wstring WidenUtf8(const std::string& s)
{
    if (s.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1)
        return L"";
    std::wstring w((size_t)n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static bool HttpGetText(const std::string& url, std::string& body, std::string& err)
{
    body.clear();
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{};
    wchar_t path[2048]{};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    const std::wstring wurl = WidenUtf8(url);
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        err = "Bad manifest URL";
        return false;
    }
    HINTERNET ses = WinHttpOpen(L"DogecoinCorePro/1.14.103",
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) {
        err = "WinHttpOpen failed";
        return false;
    }
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) {
        err = "Could not reach " + url;
        WinHttpCloseHandle(ses);
        return false;
    }
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        err = "OpenRequest failed";
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }
    BOOL sent = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    BOOL recvd = sent && WinHttpReceiveResponse(req, nullptr);
    if (!recvd) {
        err = "HTTPS request failed (CDN down or TLS blocked)";
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0)
            break;
        std::string chunk(avail, '\0');
        DWORD got = 0;
        if (!WinHttpReadData(req, &chunk[0], avail, &got))
            break;
        body.append(chunk.data(), got);
        if (body.size() > 2 * 1024 * 1024)
            break;
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    if (body.empty()) {
        err = "Empty manifest";
        return false;
    }
    return true;
}
#else
static bool HttpGetText(const std::string&, std::string&, std::string& err)
{
    err = "HTTPS manifest fetch is Windows-only in this build";
    return false;
}
#endif

static void OpenUrl(const std::string& url)
{
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string cmd = "open \"" + url + "\"";
    (void)std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
    (void)std::system(cmd.c_str());
#endif
}

static void CopyBuf(char* dest, size_t n, const std::string& s)
{
    std::snprintf(dest, n, "%s", s.c_str());
}

void App::InitBootChecklist()
{
    bootChecks = {
        {"intro", "WHO LET THE DOGE OUT?!", CheckItem::State::Active},
        {"locate", "Locate dogecoind", CheckItem::State::Pending},
        {"start", "Start dogecoind process", CheckItem::State::Pending},
        {"rpc", "Open RPC (node warmup)", CheckItem::State::Pending},
        {"index", "Load block index / chainstate", CheckItem::State::Pending},
        {"verify", "Verify / rewind blocks (if needed)", CheckItem::State::Pending},
        {"wallet", "Load wallet", CheckItem::State::Pending},
        {"network", "Start network", CheckItem::State::Pending},
        {"ready", "Ready", CheckItem::State::Pending},
    };
    bootHeadline = "WHO LET THE DOGE OUT?!";
    bootDetail = "Starting Dogecoin Core Pro...";
    lastInitMessage.clear();
    readySinceTime = 0.0;
}

void App::SetCheck(const char* id, CheckItem::State st, const char* labelOverride)
{
    for (auto& c : bootChecks) {
        if (c.id == id) {
            c.state = st;
            if (labelOverride)
                c.label = labelOverride;
            return;
        }
    }
    for (auto& c : shutdownChecks) {
        if (c.id == id) {
            c.state = st;
            if (labelOverride)
                c.label = labelOverride;
            return;
        }
    }
}

bool App::IsTestnet() const
{
    return cfg.network == "test" || snap.chain == "test";
}

void App::ParseLaunchArgs(int argc, char** argv)
{
    if (!argv)
        return;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i] ? argv[i] : "";
        if (a == "--testnet" || a == "-testnet")
            cfg.network = "test";
        else if (a == "--regtest" || a == "-regtest")
            cfg.network = "regtest";
        else if (a == "--mainnet" || a == "-mainnet")
            cfg.network = "main";
    }
}

void App::ApplyNetworkMode()
{
    FillDefaultDatadir(defaultDatadir, sizeof(defaultDatadir));
    if (cfg.network == "test") {
        rpcPort = 44555;
#if defined(_WIN32)
        std::snprintf(defaultDatadir, sizeof(defaultDatadir), "%s\\testnet3", defaultDatadir);
#else
        std::snprintf(defaultDatadir, sizeof(defaultDatadir), "%s/testnet3", defaultDatadir);
#endif
        if (theme == ProTheme::GoldDark) {
            theme = ProTheme::Matrix;
            ApplyProTheme(theme);
            cfg.theme = (int)theme;
        }
    } else if (cfg.network == "regtest") {
        rpcPort = 18332;
#if defined(_WIN32)
        std::snprintf(defaultDatadir, sizeof(defaultDatadir), "%s\\regtest", defaultDatadir);
#else
        std::snprintf(defaultDatadir, sizeof(defaultDatadir), "%s/regtest", defaultDatadir);
#endif
    } else {
        cfg.network = "main";
        rpcPort = 22555;
        if (theme == ProTheme::Matrix) {
            theme = ProTheme::GoldDark;
            ApplyProTheme(theme);
            cfg.theme = (int)theme;
        }
    }
    if (!useCustomDatadir)
        std::snprintf(datadirHint, sizeof(datadirHint), "%s", defaultDatadir);
    cfg.rpcPort = rpcPort;
    cfg.datadirHint = datadirHint;
}

bool App::Init(const std::string& assetsDir, int argc, char** argv)
{
    assetsRoot = assetsDir;
    while (!assetsRoot.empty() && (assetsRoot.back() == '/' || assetsRoot.back() == '\\'))
        assetsRoot.pop_back();
    settingsPath = assetsRoot + "/../pro-gui-settings.ini";
    nodeConfExportPath = assetsRoot + "/../pro-gui-node.conf";
    LoadAssets(assetsRoot);
    LoadOrInitSettings();
    ApplySettingsToUi();
    ParseLaunchArgs(argc, argv);
    ApplyNetworkMode();
    ApplyDefaultDatadirHint();
    ApplyRpcFromNodeConf();
    {
        std::string role = LoadInstallRole(InstallDir());
        // Hybrid/Server: X / Hide to tray must not stop the node. File→Exit and
        // tray Quit still force-stop via RequestStopNodeAndExit (service + RPC).
        if (role == "hybrid" || role == "server")
            stopNodeOnExit = false;
    }
    ApplyOfficialFastSyncDefaults();
    ApplyProTheme(theme);
    SyncRpcConfig();
    // Never launch Tor unless the user opted in (Options → Privacy).
    if (cfg.p2pViaTor) {
        const std::string exe = FindTorExecutable();
        const char* dd = datadirHint[0] ? datadirHint : defaultDatadir;
        std::string tdir = dd ? (std::string(dd) + "\\tor-data") : std::string();
        std::string err;
        StartLocalTor(exe, tdir, err);
    }
    dogecoindPath = FindBestDogecoind(assetsRoot);
    if (!dogecoindPath.empty())
        nodeLaunchStatus = "Found node: " + dogecoindPath;
    else
        nodeLaunchStatus = "dogecoind not found nearby";
    LoadBusinessInvoices();
    InitBootChecklist();
    showBootSplash = true;
    offlineMode = false;
    autoStartTried = false;
    bootStartTime = 0.0; // set on first frame when ImGui time is valid
    return true;
}

bool App::TryStartLocalNode()
{
    if (dogecoindPath.empty())
        dogecoindPath = FindBestDogecoind(assetsRoot);
    if (dogecoindPath.empty()) {
        nodeLaunchStatus = "No dogecoind.exe found";
        SetCheck("locate", CheckItem::State::Fail);
        bootHeadline = "Could not find dogecoind";
        bootDetail = nodeLaunchStatus;
        return false;
    }
    SetCheck("locate", CheckItem::State::Done);
    SetCheck("start", CheckItem::State::Active);
    bootHeadline = "Starting dogecoind...";
    bootDetail = dogecoindPath;
    nodeLaunchStatus = "Starting " + dogecoindPath;

    // One node. If IBD is already running, never start the service (that
    // launches a second dogecoind on the same datadir and kills the first).
    if (IsDogecoindRunning()) {
        nodeLaunchAttempted = true;
        weStartedNode = false;
        SetCheck("start", CheckItem::State::Done, "dogecoind already running");
        bootHeadline = "dogecoind is running";
        bootDetail = cfg.network == "test"
                         ? "Waiting for testnet RPC :44555. If this hangs, stop the mainnet node first (one dogecoind)."
                         : "Waiting for RPC / AppInit...";
        lastProbeTime = 0;
        probeWorker.Kick();
        return true;
    }

#if defined(_WIN32)
    // The Windows service is mainnet. Testnet is a separate process + testnet3 datadir.
    if (cfg.network != "test" && cfg.network != "regtest") {
        std::string svcErr;
        const CoreProServiceState svc = QueryCoreProService();
        if (svc == CoreProServiceState::Stopped || svc == CoreProServiceState::Other) {
            bootHeadline = "Starting node service...";
            bootDetail = "DogecoinGPENode (one dogecoind, no extra console)";
            if (StartCoreProService(svcErr)) {
                nodeLaunchAttempted = true;
                weStartedNode = true;
                SetCheck("start", CheckItem::State::Done, "service started");
                lastProbeTime = 0;
                probeWorker.Kick();
                return true;
            }
            nodeLaunchStatus = svcErr;
        } else if (svc == CoreProServiceState::Running) {
            nodeLaunchAttempted = true;
            weStartedNode = false;
            SetCheck("start", CheckItem::State::Done, "service already running");
            bootHeadline = "dogecoind is running";
            bootDetail = "Waiting for RPC / AppInit...";
            lastProbeTime = 0;
            probeWorker.Kick();
            return true;
        }
    }
#endif

    std::string extra;
    std::string launchDir = datadirHint;
    // Core appends testnet3/regtest to -datadir when -testnet/-regtest is set.
    // Hint/display paths are the network folder; launch must pass the parent.
    auto stripNet = [](std::string p, const char* winSuf, const char* nixSuf) {
#ifdef _WIN32
        const size_t n = std::strlen(winSuf);
        if (p.size() >= n && _stricmp(p.c_str() + p.size() - n, winSuf) == 0)
            p.resize(p.size() - n);
#else
        (void)winSuf;
        const size_t n = std::strlen(nixSuf);
        if (p.size() >= n && p.compare(p.size() - n, n, nixSuf) == 0)
            p.resize(p.size() - n);
#endif
        while (!p.empty() && (p.back() == '\\' || p.back() == '/'))
            p.pop_back();
        return p;
    };
    if (cfg.network == "test") {
        extra = "-testnet";
        launchDir = stripNet(defaultDatadir[0] ? defaultDatadir : datadirHint, "\\testnet3", "/testnet3");
    } else if (cfg.network == "regtest") {
        extra = "-regtest";
        launchDir = stripNet(defaultDatadir[0] ? defaultDatadir : datadirHint, "\\regtest", "/regtest");
    }
    if (cfg.scriptThreads < 0)
        cfg.scriptThreads = 0;
    if (cfg.scriptThreads > 16)
        cfg.scriptThreads = 16;
    if (!extra.empty())
        extra += " ";
    extra += "-par=" + std::to_string(cfg.scriptThreads);
    std::string err;
    if (!StartDogecoind(dogecoindPath, launchDir, err, LaunchPruneMiB(), cfg.dbCacheMb,
                        cfg.archivePath, cfg.preferMdbx ? "mdbx" : "", extra)) {
        if (err.find("already running") != std::string::npos) {
            nodeLaunchAttempted = true;
            SetCheck("start", CheckItem::State::Done, "dogecoind already running");
            lastProbeTime = 0;
            return true;
        }
        nodeLaunchStatus = err;
        SetCheck("start", CheckItem::State::Fail, err.c_str());
        bootHeadline = "Could not start dogecoind";
        bootDetail = err;
        return false;
    }
    nodeLaunchAttempted = true;
    weStartedNode = true;
    offlineMode = false;
    showBootSplash = true;
    SetCheck("start", CheckItem::State::Done, "dogecoind process started");
    bootHeadline = "dogecoind started";
    bootDetail = "Loading block index, wallet, network...";
    nodeLaunchStatus = "dogecoind started";
    lastProbeTime = 0;
    probeWorker.Kick();
    return true;
}

int App::LaunchPruneMiB() const
{
    if (!(cfg.preferFastSync || cfg.prune))
        return 0;
    int mb = cfg.pruneSizeGb * 1000;
    if (mb < 5500)
        mb = 5500;
    return mb;
}

void App::ApplyDefaultDatadirHint()
{
    FillDefaultDatadir(defaultDatadir, sizeof(defaultDatadir));
    if (!datadirHint[0] && cfg.datadirHint.empty()) {
        std::snprintf(datadirHint, sizeof(datadirHint), "%s", defaultDatadir);
        cfg.datadirHint = datadirHint;
    }
    useCustomDatadir = datadirHint[0] && defaultDatadir[0] && !SamePath(datadirHint, defaultDatadir);
}

bool App::NeedsFirstRunPrompt() const
{
    // Same gate as Qt Intro::pickDataDirectory(): missing folder (or never confirmed).
    // A missing wallet.dat inside an existing folder does NOT reopen this — Core just
    // creates a new wallet there, matching the original client.
    return !cfg.firstRunDone || !DirectoryExists(datadirHint);
}

void App::FinishFirstRunAndStart()
{
    firstRunError.clear();
    if (!useCustomDatadir && defaultDatadir[0])
        std::snprintf(datadirHint, sizeof(datadirHint), "%s", defaultDatadir);
    if (!datadirHint[0]) {
        firstRunError = "Choose a data directory first.";
        return;
    }
    if (LooksLikeConsumerCloudPath(datadirHint)) {
        firstRunError =
            "Do not put the live data directory on a cloud sync folder "
            "(OneDrive, Google Drive, iCloud, Dropbox, Proton Drive, pCloud, MEGA, Nextcloud). "
            "Those agents lock files and can crash or corrupt a running node. "
            "Use a local disk (the default AppData / ~/.dogecoin folder is fine). "
            "A later archive folder for finalized, read-only block files can go on cloud storage.";
        return;
    }
    if (!EnsureDirectory(datadirHint)) {
        firstRunError = "Error: Specified data directory cannot be created.";
        return;
    }
    if (cfg.preferMdbx) {
        char cur[560];
        std::snprintf(cur, sizeof(cur), "%s\\chainstate\\CURRENT", datadirHint);
        if (!FileExists(cur))
            std::snprintf(cur, sizeof(cur), "%s/chainstate/CURRENT", datadirHint);
        if (FileExists(cur)) {
            firstRunError =
                "This folder already has a LevelDB chainstate. MDBX is for a wiped or brand-new "
                "datadir only. Stop the node, move wallet.dat aside, delete chainstate/ and "
                "blocks/, then pick MDBX again. Or leave LevelDB checked.";
            return;
        }
    }
    cfg.firstRunDone = true;
    cfg.datadirHint = datadirHint;
    if (cfg.preferFastSync) {
        cfg.prune = true;
        if (cfg.pruneSizeGb < 6)
            cfg.pruneSizeGb = 6;
        if (cfg.dbCacheMb < 1024)
            cfg.dbCacheMb = 1024;
    }
    CollectUiToSettings();
    SaveSettings(settingsPath, cfg);
    {
        std::string conf = std::string(datadirHint) + "/dogecoin.conf";
        UpsertNodeConfKey(conf, "dbengine", cfg.preferMdbx ? "mdbx" : "leveldb");
        if (cfg.preferFastSync)
            UpsertNodeConfKey(conf, "prune", "5500");
    }
    autoStartTried = false;
    InitBootChecklist();
    bootStartTime = ImGui::GetTime() - 1.3;
    lastProbeTime = 0;
}

void App::ApplyOfficialFastSyncDefaults()
{
    if (!snapshotManifest[0])
        std::snprintf(snapshotManifest, sizeof(snapshotManifest),
                      "%s", "https://sync.doge.gopastearth.com/latest.json");
    if (!snapshotPath[0]) {
        if (snapshotDestPath[0]) {
#if defined(_WIN32)
            std::snprintf(snapshotPath, sizeof(snapshotPath), "%s\\utxo_fetch.dat", snapshotDestPath);
#else
            std::snprintf(snapshotPath, sizeof(snapshotPath), "%s/utxo_fetch.dat", snapshotDestPath);
#endif
        } else if (datadirHint[0] || defaultDatadir[0]) {
            const char* d = datadirHint[0] ? datadirHint : defaultDatadir;
            std::snprintf(snapshotPath, sizeof(snapshotPath), "%s\\snapshots\\utxo_fetch.dat", d);
        }
    }
    if (!snapshotShaArg[0]) {
        // Current GPE latest.json (2026-08-10 dump, height 6325931). Resolve Manifest
        // refreshes this from the live file — you do not need to type it.
        std::snprintf(snapshotShaArg, sizeof(snapshotShaArg),
                      "%s", "d2d5ee3182fa309eb6d54c582adfbd6fe6864ecd1b8256db6fc48ccb4390b1ba");
    }
    if (fastSyncArtifactUrl.empty())
        fastSyncArtifactUrl = "https://sync.doge.gopastearth.com/utxo-6325931-20260810T074725Z.dat";
    if (fastSyncHeight.empty())
        fastSyncHeight = "6325931";
    if (fastSyncBytes.empty())
        fastSyncBytes = "11498532864";
}

std::string App::WalletDatPath() const
{
    const char* d = datadirHint[0] ? datadirHint : defaultDatadir;
    if (!d || !d[0])
        return {};
    return std::string(d) + "\\wallet.dat";
}

void App::BeginImportWallet()
{
    walletStatus.clear();
#if defined(_WIN32)
    if (!PickOpenFile(walletImportSrc, sizeof(walletImportSrc), L"Import wallet.dat"))
        return;
#else
    walletStatus = "File picker is Windows-only.";
    return;
#endif
    if (!walletImportSrc[0])
        return;
    if (IsDogecoindRunning() || snap.connected) {
        std::string err;
        RequestDogecoindStop(rpcHost, rpcPort, cookiePath, rpcUser, rpcPass, err);
        walletOp = WalletOp::ImportWaitStop;
        walletOpStart = ImGui::GetTime();
        walletStatus = "Stopping the node so wallet.dat can be replaced…";
        return;
    }
    FinishImportWallet();
}

void App::BeginBackupWallet()
{
    walletStatus.clear();
    char dest[512] = "";
#if defined(_WIN32)
    if (!PickSaveFile(dest, sizeof(dest), L"Backup wallet.dat"))
        return;
#else
    walletStatus = "File picker is Windows-only.";
    return;
#endif
    if (!dest[0])
        return;
    if (snap.connected) {
        std::string p = "[\"" + JsonEscape(dest) + "\"]";
        auto r = rpc.call("backupwallet", p);
        walletStatus = r.ok ? (std::string("Backed up to ") + dest) : (r.error + "\n" + r.body);
        return;
    }
#if defined(_WIN32)
    std::string err;
    const std::string src = WalletDatPath();
    if (src.empty() || !CopyFileUtf8(src.c_str(), dest, err))
        walletStatus = err.empty() ? "No wallet.dat to copy." : err;
    else
        walletStatus = std::string("Copied to ") + dest;
#endif
}

void App::FinishImportWallet()
{
    const std::string dest = WalletDatPath();
    if (dest.empty()) {
        walletStatus = "No data directory set.";
        walletOp = WalletOp::None;
        return;
    }
    EnsureDirectory(datadirHint[0] ? datadirHint : defaultDatadir);
#if defined(_WIN32)
    std::string err;
    if (!CopyFileUtf8(walletImportSrc, dest.c_str(), err)) {
        walletStatus = err;
        walletOp = WalletOp::None;
        return;
    }
#endif
    walletStatus = "Imported wallet.dat. Starting node…";
    walletOp = WalletOp::None;
    autoStartTried = false;
    if (!TryStartLocalNode()) {
        showBootSplash = true;
        InitBootChecklist();
        bootStartTime = ImGui::GetTime() - 1.3;
        lastProbeTime = 0;
    }
}

void App::TickWalletOp()
{
    if (walletOp != WalletOp::ImportWaitStop)
        return;
    if (!IsDogecoindRunning()) {
        FinishImportWallet();
        return;
    }
    if (ImGui::GetTime() - walletOpStart > 45.0)
        walletStatus = "Still waiting for dogecoind to stop… do not copy wallet.dat while it is running.";
}

void App::StartResolveManifest()
{
    if (fastSyncBusy)
        return;
    fastSyncBusy = true;
    {
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = std::string("Resolving ") + snapshotManifest + " …";
    }
    if (fastSyncThread.joinable())
        fastSyncThread.join();
    const std::string url = snapshotManifest;
    fastSyncThread = std::thread([this, url]() {
        std::string body, err;
        const bool ok = HttpGetText(url, body, err);
        std::lock_guard<std::mutex> lock(fastSyncMu);
        if (!ok) {
            fastSyncStatus = "Manifest fetch failed: " + err;
        } else {
            pendingManifestBody = body;
            pendingManifestReady = true;
            fastSyncStatus = "Manifest OK — fields updated from latest.json.";
        }
        fastSyncBusy = false;
    });
}

void App::StartDownloadSnapshot()
{
    if (fastSyncBusy)
        return;
    if (snap.blocks > 10000 && (cfg.prune || cfg.preferFastSync)) {
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus =
            "Fast Sync refused: this node is mid-sync past height 10,000 with prune.\n"
            "Use a fresh data directory, then Resolve manifest + Download snapshot from height 0.";
        return;
    }
    fastSyncBusy = true;
    {
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus =
            "Downloading attested snapshot (about 11 GB). This window stays usable.\n"
            "Watch %APPDATA%\\Dogecoin\\debug.log for stream-hash progress.";
    }
    if (fastSyncThread.joinable())
        fastSyncThread.join();
    const std::string manifest = snapshotManifest;
    const RpcConfig c = rpc.config();
    fastSyncThread = std::thread([this, manifest, c]() {
        RpcClient client(c);
        const std::string p = "[\"" + JsonEscape(manifest) + "\"]";
        auto r = client.call("fetchassumeutxomanifest", p);
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = r.ok ? r.resultJson : (r.error + "\n" + r.body);
        if (r.ok)
            pendingManifestReady = false;
        fastSyncBusy = false;
    });
}

void App::StartLoadSnapshot(bool activate)
{
    if (fastSyncBusy || !snapshotPath[0])
        return;
    fastSyncBusy = true;
    {
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = activate ? "Loading + activating snapshot…" : "Loading snapshot (no activate)…";
    }
    if (fastSyncThread.joinable())
        fastSyncThread.join();
    const std::string path = snapshotPath;
    const RpcConfig c = rpc.config();
    fastSyncThread = std::thread([this, path, activate, c]() {
        RpcClient client(c);
        const std::string p = "[\"" + JsonEscape(path) + "\", " + (activate ? "true" : "false") + "]";
        auto r = client.call("loadtxoutset", p);
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = r.ok ? r.resultJson : (r.error + "\n" + r.body);
        fastSyncBusy = false;
    });
}

void App::StartArchiveRpc(const char* method, const std::string& paramsJson)
{
    if (fastSyncBusy || !method || !method[0])
        return;
    fastSyncBusy = true;
    {
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = std::string(method) + "…";
    }
    if (fastSyncThread.joinable())
        fastSyncThread.join();
    const std::string m = method;
    const std::string p = paramsJson.empty() ? "[]" : paramsJson;
    const RpcConfig c = rpc.config();
    fastSyncThread = std::thread([this, m, p, c]() {
        RpcClient client(c);
        auto r = client.call(m, p);
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = r.ok ? r.resultJson : (r.error + "\n" + r.body);
        fastSyncBusy = false;
    });
}

void App::TickFastSync()
{
    std::lock_guard<std::mutex> lock(fastSyncMu);
    if (!pendingManifestReady)
        return;
    pendingManifestReady = false;
    const std::string sha = JsonField(pendingManifestBody, "sha256");
    const std::string url = JsonField(pendingManifestBody, "url");
    const std::string blocks = JsonField(pendingManifestBody, "blocks");
    const std::string bytes = JsonField(pendingManifestBody, "bytes");
    if (!sha.empty())
        std::snprintf(snapshotShaArg, sizeof(snapshotShaArg), "%s", sha.c_str());
    if (!url.empty())
        fastSyncArtifactUrl = url;
    if (!blocks.empty())
        fastSyncHeight = blocks;
    if (!bytes.empty())
        fastSyncBytes = bytes;
    char summary[768];
    std::snprintf(summary, sizeof(summary),
                  "Official CDN latest.json\n"
                  "  height  %s\n"
                  "  size    %s bytes (~11 GB)\n"
                  "  file    %s\n"
                  "  sha256  %s\n"
                  "Local load path after download:\n  %s",
                  fastSyncHeight.c_str(),
                  fastSyncBytes.c_str(),
                  fastSyncArtifactUrl.c_str(),
                  snapshotShaArg,
                  snapshotPath);
    fastSyncStatus = summary;
}

void App::ApplyP2pTorPref(bool on)
{
    cfg.p2pViaTor = on;
    if (on) {
        cfg.proxyEnabled = true;
        cfg.proxyIp = "127.0.0.1";
        cfg.proxyPort = 9050;
        std::snprintf(proxyIp, sizeof(proxyIp), "127.0.0.1");
        const std::string exe = FindTorExecutable();
        const char* dd = datadirHint[0] ? datadirHint : defaultDatadir;
        std::string tdir = dd ? (std::string(dd) + "\\tor-data") : std::string();
        std::string err;
        if (!StartLocalTor(exe, tdir, err) && !err.empty())
            optionsStatus = err;
    } else {
        cfg.proxyEnabled = false;
    }
    CollectUiToSettings();
    SaveSettings(settingsPath, cfg);
    const char* dd = datadirHint[0] ? datadirHint : defaultDatadir;
    if (dd && dd[0]) {
        const std::string conf = std::string(dd) + "/dogecoin.conf";
        if (on) {
            UpsertNodeConfKey(conf, "proxy", "127.0.0.1:9050");
            UpsertNodeConfKey(conf, "listenonion", "0");
        } else {
            UpsertNodeConfKey(conf, "proxy", "0");
        }
    }
    if (optionsStatus.empty())
        optionsStatus = on ? "P2P-via-Tor saved to dogecoin.conf (applies on next node start)."
                           : "Cleared proxy= in dogecoin.conf (applies on next node start).";
}

void App::RequestStopNodeAndExit()
{
    forceStopNodeOnExit = true;
    RequestExit();
}

void App::RequestExit()
{
    if (shuttingDown || shutdownComplete)
        return;
    hiddenInTray = false;
    trayHintPending = false;
    if (hostWindow) {
        glfwShowWindow(hostWindow);
        glfwFocusWindow(hostWindow);
    }
    ApplyRpcFromNodeConf();
    shuttingDown = true;
    showBootSplash = false;
    shutdownStartTime = ImGui::GetTime();
    stopRequested = false;
    shutdownComplete = false;
    wantsClose = false;
    const bool stopNode = stopNodeOnExit || forceStopNodeOnExit;
    if (!stopNode) {
        shutdownHeadline = "Closing the desktop UI...";
        shutdownDetail = "dogecoind stays running. Reopen from the system tray.";
        shutdownChecks = {
            {"gui", "Close control plane panels", CheckItem::State::Done},
            {"rpcstop", "Leave dogecoind running", CheckItem::State::Skip},
            {"flush", "No flush — node still up", CheckItem::State::Skip},
            {"exit", "GUI will exit", CheckItem::State::Pending},
        };
        return;
    }
    shutdownHeadline = "Dogecoin Core is shutting down...";
    shutdownDetail = "Please wait while the node flushes safely.";
    shutdownChecks = {
        {"gui", "Close control plane panels", CheckItem::State::Done},
        {"rpcstop", "Ask dogecoind to stop (RPC stop)", CheckItem::State::Pending},
        {"flush", "Flush wallet / chainstate / close DB", CheckItem::State::Pending},
        {"exit", "Node process exited", CheckItem::State::Pending},
    };
}

void App::LoadOrInitSettings()
{
    if (!LoadSettings(settingsPath, cfg)) {
        cfg = ProGuiSettings();
        SaveSettings(settingsPath, cfg);
    }
}

void App::ApplySettingsToUi()
{
    CopyBuf(rpcHost, sizeof(rpcHost), cfg.rpcHost);
    rpcPort = cfg.rpcPort;
    CopyBuf(rpcUser, sizeof(rpcUser), cfg.rpcUser);
    CopyBuf(cookiePath, sizeof(cookiePath), cfg.cookiePath);
    CopyBuf(snapshotUrl, sizeof(snapshotUrl), cfg.snapshotUrl);
    CopyBuf(snapshotSha, sizeof(snapshotSha), cfg.snapshotSha256);
    CopyBuf(proxyIp, sizeof(proxyIp), cfg.proxyIp);
    CopyBuf(proxyTorIp, sizeof(proxyTorIp), cfg.proxyTorIp);
    CopyBuf(datadirHint, sizeof(datadirHint), cfg.datadirHint);
    CopyBuf(archivePath, sizeof(archivePath), cfg.archivePath);
    CopyBuf(snapshotDestPath, sizeof(snapshotDestPath), cfg.snapshotDestPath);
    CopyBuf(displayUnit, sizeof(displayUnit), cfg.displayUnit);
    CopyBuf(memeAuthor, sizeof(memeAuthor), cfg.memeAuthor);
    theme = (ProTheme)cfg.theme;
    EnsureHomePanels();
    {
        std::string inst = settingsPath;
        auto slash = inst.find_last_of("/\\");
        if (slash != std::string::npos)
            inst = inst.substr(0, slash);
        cfg.hybridDefaultUi = LoadHybridUiPref(inst);
    }
}

void App::CollectUiToSettings()
{
    cfg.rpcHost = rpcHost;
    cfg.rpcPort = rpcPort;
    cfg.rpcUser = rpcUser;
    cfg.rpcPassword.clear();
    cfg.cookiePath = cookiePath;
    cfg.snapshotUrl = snapshotUrl;
    cfg.snapshotSha256 = snapshotSha;
    cfg.proxyIp = proxyIp;
    cfg.proxyTorIp = proxyTorIp;
    cfg.datadirHint = datadirHint;
    cfg.archivePath = archivePath;
    cfg.snapshotDestPath = snapshotDestPath;
    cfg.displayUnit = displayUnit;
    cfg.memeAuthor = memeAuthor;
    cfg.theme = (int)theme;
    PersistHomePanels();
    {
        std::string inst = settingsPath;
        auto slash = inst.find_last_of("/\\");
        if (slash != std::string::npos)
            inst = inst.substr(0, slash);
        SaveHybridUiPref(inst, cfg.hybridDefaultUi);
    }
}

std::string App::InstallDir() const
{
    std::string inst = settingsPath;
    auto slash = inst.find_last_of("/\\");
    if (slash != std::string::npos)
        inst = inst.substr(0, slash);
    return inst;
}

void App::ApplyRpcFromNodeConf()
{
    std::string u, p;
    int port = 0;
    if (!LoadNodeRpcCredentials(datadirHint, u, p, &port))
        return;
    if (!u.empty())
        CopyBuf(rpcUser, sizeof(rpcUser), u);
    if (!p.empty())
        CopyBuf(rpcPass, sizeof(rpcPass), p);
    if (port > 0)
        rpcPort = port;
    if (!cookiePath[0]) {
        const char* dd = datadirHint[0] ? datadirHint : defaultDatadir;
        if (dd && dd[0]) {
            std::string c = std::string(dd) + "\\.cookie";
            CopyBuf(cookiePath, sizeof(cookiePath), c.c_str());
        }
    }
    SyncRpcConfig();
}

void App::SyncRpcConfig()
{
    RpcConfig c;
    c.host = rpcHost;
    c.port = rpcPort;
    c.user = rpcUser;
    c.password = rpcPass;
    c.cookiePath = cookiePath;
    rpc.setConfig(c);
    probeWorker.SetConfig(c);
}

void App::LoadAssets(const std::string& root)
{
    auto path = [&](const std::string& rel) { return root + "/" + rel; };
    struct Spec {
        const char* id;
        const char* label;
        const char* file;
    };
    const Spec specs[] = {
        {"home", "Home", "sidebar/homeSidebarIcon.png"},
        {"send", "Send / Receive", "sidebar/sendSidebarIcon.png"},
        {"tx", "History", "sidebar/txSidebarIcon.png"},
        {"network", "Network", "sidebar/networkSidebarIcon.png"},
        {"blocks", "Chain", "sidebar/blocksSidebarIcon.png"},
        {"arcade", "Arcade", "sidebar/dogeArcadeSidebarIcon.png"},
        {"meme", "Meme Stream", "sidebar/memeStreamSidebarIcon.png"},
        {"business", "Business", "sidebar/busCenterSidebarIcon.png"},
        {"console", "Console", "sidebar/consoleSidebarIcon.png"},
        {"mining", "Mining", "sidebar/miningSidebarIcon.png"},
        {"settings", "Options", "sidebar/settingsSidebarIcon.png"},
    };
    nav.clear();
    for (const auto& s : specs) {
        NavItem n;
        n.id = s.id;
        n.label = s.label;
        n.file = s.file;
        n.tex = LoadTextureFromFile(path(s.file));
        nav.push_back(n);
    }
    auto loadIcon = [&](const char* key, const char* rel) {
        icons[key] = LoadTextureFromFile(path(rel));
    };
    loadIcon("balance", "icons/walletballanceIcon.png");
    loadIcon("send", "icons/sendIcon.png");
    loadIcon("receive", "icons/receiveIcon.png");
    loadIcon("fastsync", "icons/fastsyncRocketIcon.png");
    loadIcon("peers_map", "icons/peersMapIcon.png");
    loadIcon("theme", "icons/themeSwitcherIcon.png");
    loadIcon("lock", "icons/lockEncrypticon.png");
    loadIcon("qr", "icons/qrStyleIcon.png");
    loadIcon("copy", "icons/copyClipboardIcon.png");
    loadIcon("success", "status/successIcon.png");
    loadIcon("error", "status/errorIcon.png");
    loadIcon("close", "chrome/closeIcon.png");
    loadIcon("minimize", "chrome/minimizeIcon.png");
    loadIcon("insert_coin", "arcade/insertCoin.png");
    loadIcon("play", "arcade/arcadecontrolPlay.png");
    loadIcon("pause", "arcade/arcadecontrolPause.png");
    splash = LoadTextureFromFile(path("brand/shibaInuonDSplash.png"));
    brandMark = LoadTextureFromFile(path("brand/dogeD_metallicGold.png"));
    worldMap = LoadTextureFromFile(path("network/worldMap.png"));

    std::ifstream cat(path("catalog.json"));
    if (cat) {
        std::stringstream ss;
        ss << cat.rdbuf();
        std::string j = ss.str();
        auto grab = [&](const char* key, std::string& out) {
            std::string k = std::string("\"") + key + "\"";
            auto p = j.find(k);
            if (p == std::string::npos) return;
            p = j.find(':', p);
            if (p == std::string::npos) return;
            p = j.find('"', p);
            if (p == std::string::npos) return;
            auto e = j.find('"', p + 1);
            if (e == std::string::npos) return;
            out = j.substr(p + 1, e - p - 1);
        };
        grab("arcade_hub_url", arcadeHubUrl);
        grab("arcade_hub_alt", arcadeHubAlt);
    }
}

void App::SetHostWindow(GLFWwindow* window)
{
    hostWindow = window;
#if defined(_WIN32)
    if (window) {
        HWND hwnd = glfwGetWin32Window(window);
        bool hybrid = (LoadInstallRole(InstallDir()) == "hybrid");
        WinTrayInit(hwnd, hybrid, IsTestnet());
        WinTrayDismissHelper();
        WinTrayShow();
    }
#else
    LinuxTrayInit();
#endif
    probeWorker.SetWake(+[]() { glfwPostEmptyEvent(); });
    probeWorker.SetConfig(rpc.config());
    probeWorker.Start();
    probeWorker.Kick();
}

void App::DoHideToTray()
{
    if (!hostWindow)
        return;
    hiddenInTray = true;
    probeWorker.SetBackground(true);
    glfwHideWindow(hostWindow);
#if defined(_WIN32)
    WinTrayDismissHelper();
    WinTrayShow();
    if (cfg.showTrayNotifications && !cfg.trayHintDontShowAgain)
        WinTrayNotify("Still running",
                      "Core Pro is in the system tray. The node is not closed.");
#else
    LinuxTrayWritePid();
    if (LoadInstallRole(InstallDir()) != "server")
        LinuxTrayEnsureHelper(InstallDir().c_str());
    if (cfg.showTrayNotifications && !cfg.trayHintDontShowAgain)
        LinuxTrayNotify("Still running",
                        "Core Pro is in the system tray. The node is not closed.");
#endif
}

void App::RequestHideToTray()
{
    if (shuttingDown)
        return;
    if (!hostWindow) {
        RequestExit();
        return;
    }
    if (hiddenInTray)
        return;
    if (!cfg.trayHintDontShowAgain) {
        trayHintPending = true;
        return;
    }
    DoHideToTray();
}

void App::DrawTrayHintModal()
{
    if (!trayHintPending)
        return;
    ImGui::OpenPopup("##trayhint");
    ImVec2 c = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##trayhint", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextUnformatted("Still running");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextWrapped(
            "Dogecoin Core Pro is going to the system tray. This is not a full close "
            "and the node is not being stopped.");
        ImGui::TextWrapped(
            "Open it again from the same tray icon (one icon). Hybrid: Show Desktop GUI "
            "or Open Operator TUI. To stop the node, use File → Exit or tray "
            "Quit and stop node.");
        ImGui::Dummy(ImVec2(0, 8));
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            trayHintPending = false;
            ImGui::CloseCurrentPopup();
            DoHideToTray();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't show again", ImVec2(168, 0))) {
            cfg.trayHintDontShowAgain = true;
            CollectUiToSettings();
            SaveSettings(settingsPath, cfg);
            trayHintPending = false;
            ImGui::CloseCurrentPopup();
            DoHideToTray();
        }
        ImGui::EndPopup();
    }
}

void App::RestoreFromTray()
{
    if (!hostWindow)
        return;
    hiddenInTray = false;
    probeWorker.SetBackground(false);
    glfwShowWindow(hostWindow);
    glfwFocusWindow(hostWindow);
#if defined(_WIN32)
    WinTrayShow();
#endif
}

void App::TickTray()
{
#if defined(_WIN32)
    if (WinTrayPollShow())
        RestoreFromTray();
    if (WinTrayPollQuit())
        RequestStopNodeAndExit();
    if (WinTrayPollOpenTui()) {
        std::string tui = InstallDir() + "\\gpenode-tui.exe";
        StartProGui(tui, nodeLaunchStatus);
    }
#else
    if (LinuxTrayPollShow())
        RestoreFromTray();
#endif
    if (hiddenInTray && hostWindow && glfwGetWindowAttrib(hostWindow, GLFW_VISIBLE))
        hiddenInTray = false;
    const bool iconified = hostWindow && glfwGetWindowAttrib(hostWindow, GLFW_ICONIFIED);
    probeWorker.SetBackground(hiddenInTray || iconified);
}

void App::Shutdown()
{
    probeWorker.Stop();
#if defined(_WIN32)
    WinTrayShutdown();
#else
    LinuxTrayClearPid();
#endif
    if (fastSyncThread.joinable())
        fastSyncThread.detach();
    ++memeJobGen;
    if (memeThread.joinable())
        memeThread.detach();
    for (auto& kv : memeThumbs)
        DestroyTexture(kv.second);
    memeThumbs.clear();
    DestroyTexture(memeImagePreview);
    arcadeWeb.Shutdown();
    CollectUiToSettings();
    SaveSettings(settingsPath, cfg);
    SaveBusinessInvoices();
    for (auto& n : nav)
        DestroyTexture(n.tex);
    nav.clear();
    for (auto& kv : icons)
        DestroyTexture(kv.second);
    icons.clear();
    DestroyTexture(splash);
    DestroyTexture(brandMark);
    DestroyTexture(worldMap);
}

void App::UpdateArcadeEmbed()
{
    const bool onArcade = !showBootSplash && !shuttingDown && activeNav == "arcade";
    const bool onMemePage = !showBootSplash && !shuttingDown && activeNav == "meme" &&
                            memeTab == 0;
    const bool onHomeMeme = !showBootSplash && !shuttingDown && activeNav == "home" &&
                            memeWebOnHome && memeWebW > 40 && memeWebH > 80;
    const bool wantWeb = onArcade || onMemePage || onHomeMeme;
    if (!wantWeb) {
        if (arcadeWeb.visible())
            arcadeWeb.Show(false);
        return;
    }

#if defined(_WIN32)
    if (!hostWindow)
        return;
    HWND hwnd = glfwGetWin32Window(hostWindow);
    if (!hwnd)
        return;

    const bool memeUrl = onMemePage || onHomeMeme;
    const std::string url = memeUrl
        ? memeStreamUrl
        : (arcadeHubUrl.empty() ? ArcadeHost::kHubUrl : arcadeHubUrl);

    if (!arcadeNavStarted) {
        arcadeNavStarted = true;
        if (!arcadeWeb.Init(hwnd)) {
            arcadeEmbedStatus = arcadeWeb.status();
            statusLine = arcadeEmbedStatus;
            return;
        }
        embedNavUrl = url;
        arcadeWeb.Navigate(url);
    } else if (arcadeWeb.ready() && embedNavUrl != url) {
        embedNavUrl = url;
        arcadeWeb.Navigate(url);
    }

    int cw = 0, ch = 0;
    glfwGetWindowSize(hostWindow, &cw, &ch);
    int x, y, w, h;
    if (onHomeMeme) {
        x = memeWebX;
        y = memeWebY;
        w = memeWebW;
        h = memeWebH;
    } else {
        const int chrome = onMemePage ? 72 : 0;
        x = (int)kRailW;
        y = (int)kCaptionH + chrome;
        w = cw - x;
        h = ch - y;
    }
    if (w > 8 && h > 8)
        arcadeWeb.SetBounds(x, y, w, h);

    if (!arcadeWeb.visible())
        arcadeWeb.Show(true);

    arcadeEmbedStatus = arcadeWeb.status();
#else
    (void)onArcade;
    (void)onMemePage;
    (void)onHomeMeme;
#endif
}

void App::OpenArcadeHub()
{
    // Prefer in-panel WebView2; only fall back to external if embed unavailable
    activeNav = "arcade";
#if defined(_WIN32)
    if (arcadeWeb.ready()) {
        const std::string url = arcadeHubUrl.empty() ? ArcadeHost::kHubUrl : arcadeHubUrl;
        arcadeWeb.Navigate(url);
        arcadeWeb.Show(true);
        statusLine = "Reloading cabinet in-panel: " + url;
        return;
    }
#endif
    const std::string url = arcadeHubUrl.empty() ? ArcadeHost::kHubUrl : arcadeHubUrl;
    if (ArcadeHost::LaunchCabinetUnlocked(url))
        statusLine = "Arcade (external, Core Pro UA): " + url;
    else {
        OpenUrl(url);
        statusLine = "Opened arcade (may stay LOCKED): " + url;
    }
}

void App::OpenGameUrl(const std::string& url)
{
    if (url.find("gopastearth.com") != std::string::npos || url.find("arcade.") != std::string::npos) {
        OpenArcadeHub();
        return;
    }
    OpenUrl(url);
    statusLine = "Launched: " + url;
}

void App::CopyToClipboard(const std::string& text)
{
    if (text.empty())
        return;
    ImGui::SetClipboardText(text.c_str());
    statusLine = "Copied to clipboard";
}

void App::RefreshHelpCatalog()
{
    helpCommands.clear();
    helpDetail.clear();
    if (!snap.connected) return;
    auto r = rpc.call("help", "[]");
    if (!r.ok) {
        helpDetail = r.error;
        return;
    }
    // help returns a string with one command per line
    std::string body = r.resultJson;
    if (body.size() >= 2 && body.front() == '"') {
        // unquote rough
        body = body.substr(1, body.size() - 2);
        // unescape \n
        std::string u;
        for (size_t i = 0; i < body.size(); ++i) {
            if (body[i] == '\\' && i + 1 < body.size()) {
                if (body[i + 1] == 'n') {
                    u.push_back('\n');
                    ++i;
                    continue;
                }
                if (body[i + 1] == '"') {
                    u.push_back('"');
                    ++i;
                    continue;
                }
                if (body[i + 1] == '\\') {
                    u.push_back('\\');
                    ++i;
                    continue;
                }
            }
            u.push_back(body[i]);
        }
        body = u;
    }
    std::istringstream iss(body);
    std::string line;
    while (std::getline(iss, line)) {
        // first token is command name
        if (line.empty() || line[0] == '=' || line[0] == ' ') continue;
        auto sp = line.find(' ');
        std::string cmd = (sp == std::string::npos) ? line : line.substr(0, sp);
        if (!cmd.empty() && cmd[0] != '-')
            helpCommands.push_back(cmd);
    }
    std::sort(helpCommands.begin(), helpCommands.end());
    helpCommands.erase(std::unique(helpCommands.begin(), helpCommands.end()), helpCommands.end());
    helpLoaded = true;
    helpDetail = "Loaded " + std::to_string(helpCommands.size()) + " commands. Select one or type: help <name>";
}

void App::RunConsoleLine(const std::string& line)
{
    if (line.empty()) return;
    consoleHistory.push_back(line);
    consoleHistoryIdx = -1;

    // Parse: "method" or "method arg1 arg2" or use params JSON field
    std::string method = line;
    std::string params = consoleParams;
    auto sp = line.find(' ');
    if (sp != std::string::npos) {
        method = line.substr(0, sp);
        std::string rest = line.substr(sp + 1);
        // if rest looks like JSON array, use it; else help style single string arg
        if (!rest.empty() && rest[0] == '[')
            params = rest;
        else if (method == "help" && !rest.empty()) {
            params = "[\"" + rest + "\"]";
        } else if (!rest.empty()) {
            // quote as single string param if not number
            bool num = true;
            for (char c : rest)
                if (!(std::isdigit((unsigned char)c) || c == '.' || c == '-')) num = false;
            if (num)
                params = "[" + rest + "]";
            else
                params = "[\"" + rest + "\"]";
        }
    }

    if (!snap.connected) {
        consoleOut = "Not connected to dogecoind.";
        return;
    }
    auto r = rpc.call(method, params);
    if (r.ok)
        consoleOut = r.resultJson;
    else
        consoleOut = "Error: " + r.error + "\n" + r.body;
    statusLine = "console: " + method;
}

void App::AdvanceBootFromSignals()
{
    // Map core InitMessage text (debug.log) + RPC state onto checklist
    const std::string& m = lastInitMessage;
    auto has = [&](const char* s) { return m.find(s) != std::string::npos; };

    if (has("Loading block index") || has("Loading block")) {
        SetCheck("rpc", CheckItem::State::Done);
        SetCheck("index", CheckItem::State::Active, "Loading block index...");
        bootHeadline = "Loading block index...";
    }
    if (has("Rewinding blocks")) {
        SetCheck("index", CheckItem::State::Done);
        SetCheck("verify", CheckItem::State::Active, "Rewinding blocks...");
        bootHeadline = "Rewinding blocks...";
    }
    if (has("Verifying blocks")) {
        SetCheck("index", CheckItem::State::Done);
        SetCheck("verify", CheckItem::State::Active, "Verifying blocks...");
        bootHeadline = "Verifying blocks...";
    }
    if (has("Pruning blockstore")) {
        SetCheck("verify", CheckItem::State::Active, "Pruning blockstore...");
        bootHeadline = "Pruning blockstore...";
    }
    if (has("Verifying wallet") || has("Loading wallet")) {
        SetCheck("verify", CheckItem::State::Done);
        SetCheck("wallet", CheckItem::State::Active, has("Verifying wallet") ? "Verifying wallet..." : "Loading wallet...");
        bootHeadline = has("Verifying wallet") ? "Verifying wallet..." : "Loading wallet...";
    }
    if (has("Rescanning")) {
        SetCheck("wallet", CheckItem::State::Active, "Rescanning wallet...");
        bootHeadline = "Rescanning...";
    }
    if (has("Loading addresses") || has("Loading banlist") || has("Starting network")) {
        SetCheck("wallet", CheckItem::State::Done);
        SetCheck("network", CheckItem::State::Active, "Starting network...");
        bootHeadline = "Starting network...";
    }
    if (has("Done loading")) {
        SetCheck("network", CheckItem::State::Done);
        bootHeadline = "Done loading";
    }
    if (!m.empty())
        bootDetail = m;
}

void App::TickBoot()
{
    const double now = ImGui::GetTime();
    if (bootStartTime <= 0.0)
        bootStartTime = now;

    // Intro beat, then auto-start node (product desktop icon path)
    if (now - bootStartTime < 1.2) {
        SetCheck("intro", CheckItem::State::Active);
        bootHeadline = "WHO LET THE DOGE OUT?!";
        bootDetail = "Dogecoin Core Pro is waking up...";
        return;
    }
    SetCheck("intro", CheckItem::State::Done);

    // Same rule as Qt Intro::pickDataDirectory(): missing folder => Welcome picker first.
    if (NeedsFirstRunPrompt()) {
        cfg.firstRunDone = false;
        bootHeadline = "Welcome to Dogecoin Core.";
        bootDetail = "Choose where Dogecoin Core will store its data.";
        return;
    }

    if (!autoStartTried) {
        autoStartTried = true;
        SetCheck("locate", CheckItem::State::Active);
        if (dogecoindPath.empty())
            dogecoindPath = FindBestDogecoind(assetsRoot);
        if (dogecoindPath.empty()) {
            SetCheck("locate", CheckItem::State::Fail);
            bootHeadline = "dogecoind not found";
            bootDetail = "Place dogecoind.exe next to this app and relaunch.";
        } else {
            SetCheck("locate", CheckItem::State::Done);
            TryStartLocalNode();
        }
    }

    // Poll debug.log for real InitMessage strings (same text Qt splash shows)
    if (now - lastLogPoll >= 0.75) {
        lastLogPoll = now;
        std::string msg = TailInitMessageFromDebugLog(DefaultDebugLogPath(cfg.network));
        if (!msg.empty() && msg != lastInitMessage) {
            lastInitMessage = msg;
            AdvanceBootFromSignals();
        }
    }

    TickNodeProbe();
}

void App::TickShutdown()
{
    const double now = ImGui::GetTime();
    const bool stopNode = stopNodeOnExit || forceStopNodeOnExit;
    if (!stopNode) {
        SetCheck("rpcstop", CheckItem::State::Skip, "Leave node running");
        SetCheck("flush", CheckItem::State::Skip);
        if (now - shutdownStartTime > 0.7) {
            SetCheck("exit", CheckItem::State::Done, "GUI exiting");
            shutdownComplete = true;
            wantsClose = true;
        }
        return;
    }

    if (!stopRequested) {
        stopRequested = true;
        // RPC stop FIRST so the coins cache + block index flush to disk.
        // SCM-stop first used to kill dogecoind at 60s mid-flush (IBD restarts
        // from genesis). After the process exits, stop the service so it
        // cannot auto-restart.
        SetCheck("rpcstop", CheckItem::State::Active);
        shutdownHeadline = "Stopping dogecoind...";
        shutdownDetail = "Sending RPC stop — node will flush wallet and databases.";
        ApplyRpcFromNodeConf();
        std::string err;
        bool ok = RequestDogecoindStop(rpcHost, rpcPort, cookiePath, rpcUser, rpcPass, err);
        if (ok) {
            SetCheck("rpcstop", CheckItem::State::Done, "stop requested");
            SetCheck("flush", CheckItem::State::Active, "Flushing / closing chainstate...");
            shutdownDetail = "Node is flushing chainstate and wallet. Do not force-kill.";
        } else if (!IsDogecoindRunning()) {
            SetCheck("rpcstop", CheckItem::State::Done, "already stopped");
            SetCheck("flush", CheckItem::State::Done);
        } else {
            SetCheck("rpcstop", CheckItem::State::Fail, err.c_str());
            shutdownDetail = err.empty() ? "Could not request stop" : err;
        }
        return;
    }

    // Wait for process exit (proper DB close)
    if (!IsDogecoindRunning()) {
#if defined(_WIN32)
        std::string svcErr;
        StopCoreProServiceWait(20000, svcErr);
#endif
        SetCheck("flush", CheckItem::State::Done);
        SetCheck("exit", CheckItem::State::Done, "dogecoind exited cleanly");
        shutdownHeadline = "Shutdown complete";
        shutdownDetail = "Node stopped. Safe to close.";
        shutdownComplete = true;
        wantsClose = true;
        return;
    }

    SetCheck("flush", CheckItem::State::Active);
    shutdownHeadline = "Waiting for dogecoind to exit...";
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "Flushing databases... (%.0fs). Do not force-kill — that restarts IBD.",
                  now - shutdownStartTime);
    shutdownDetail = buf;

    // Large IBD coins caches need minutes, not seconds.
    if (now - shutdownStartTime > 900.0) {
        SetCheck("exit", CheckItem::State::Fail, "still flushing after 15m — leave it running");
        shutdownHeadline = "Shutdown is taking longer than expected";
        shutdownDetail = "dogecoind is still flushing. Leave it; force-kill will lose sync progress.";
        shutdownComplete = true;
        wantsClose = true;
    }
}

void App::TickNodeProbe()
{
    const double now = ImGui::GetTime();
    ApplyRpcFromNodeConf();
    NodeSnapshot next;
    if (!probeWorker.Consume(next))
        return;
    lastProbeTime = now;
    HoldLastGoodOnProbeMiss(next, snap);
    snap = std::move(next);

    if (snap.connected && snap.blocks > 0) {
        const bool jumped = snap.blocks > cfg.lastKnownHeight + 250 ||
                            cfg.lastKnownHeight == 0;
        cfg.lastKnownHeight = snap.blocks;
        if (snap.headers > cfg.lastKnownHeaders)
            cfg.lastKnownHeaders = snap.headers;
        if (jumped && now - lastHeightSaveTime > 20.0) {
            lastHeightSaveTime = now;
            CollectUiToSettings();
            SaveSettings(settingsPath, cfg);
        }
    }

    if (!snap.connected) {
        if (showBootSplash) {
            if (nodeLaunchAttempted && !IsDogecoindRunning()) {
                bootHeadline = "Node stopped unexpectedly";
                bootDetail = "Check debug.log (prune/datadir). Then Start node again.";
                SetCheck("start", CheckItem::State::Fail, "process exited");
#if defined(_WIN32)
                if (!rpcStuckRestartTried &&
                    QueryCoreProService() != CoreProServiceState::Missing &&
                    lastInitMessage.find("Error opening block database") == std::string::npos &&
                    lastInitMessage.find("Aborted block database") == std::string::npos) {
                    rpcStuckRestartTried = true;
                    std::string err;
                    if (StartCoreProService(err)) {
                        bootHeadline = "Restarted node service";
                        bootDetail = "Waiting for RPC after service start...";
                        SetCheck("start", CheckItem::State::Active, "service restarted");
                    }
                }
#endif
            } else if (nodeLaunchAttempted) {
                SetCheck("rpc", CheckItem::State::Active, "Waiting for RPC port...");
                if (bootHeadline.find("WHO LET") == std::string::npos)
                    bootHeadline = "Waiting for RPC...";
#if defined(_WIN32)
                // Process is up but RPC never arrived (hung AppInit / leftover
                // orphan). After a minute, restart the Windows service once.
                if (!rpcStuckRestartTried && !snap.rpcWarmup &&
                    bootStartTime > 0 && (now - bootStartTime) > 60.0 &&
                    QueryCoreProService() != CoreProServiceState::Missing) {
                    rpcStuckRestartTried = true;
                    bootHeadline = "RPC not ready — restarting node service";
                    bootDetail = "Asking dogecoind to stop, then starting DogecoinGPENode.";
                    std::string err;
                    if (RestartCoreProNode(rpcHost, rpcPort, cookiePath, rpcUser, rpcPass, err)) {
                        SetCheck("start", CheckItem::State::Active, "service restarted");
                        bootDetail = "Service restarted. Waiting for RPC...";
                    } else if (!err.empty()) {
                        bootDetail = err;
                    }
                }
#endif
            }
        }
        if (snap.rpcWarmup) {
            SetCheck("start", CheckItem::State::Done);
            SetCheck("rpc", CheckItem::State::Active, "RPC warmup (AppInit)...");
            bootHeadline = "Loading...";
            if (lastInitMessage.empty())
                bootDetail = "Node accepted RPC but is still in AppInit (index/wallet/network).";
        }
        if (!offlineMode)
            statusLine = bootHeadline;
        return;
    }

    // Fully connected — mark remaining checks done
    SetCheck("rpc", CheckItem::State::Done);
    SetCheck("index", CheckItem::State::Done);
    SetCheck("verify", CheckItem::State::Done);
    if (snap.hasWallet)
        SetCheck("wallet", CheckItem::State::Done, "Wallet loaded");
    else
        SetCheck("wallet", CheckItem::State::Skip, "No wallet RPC (-disablewallet?)");
    SetCheck("network", CheckItem::State::Done);
    SetCheck("ready", CheckItem::State::Done);
    bootHeadline = "Ready";
    char ready[128];
    std::snprintf(ready, sizeof(ready), "height %d · peers %d", snap.blocks, snap.peerCount);
    bootDetail = ready;
    statusLine = snap.status;

    // Offline continue: still auto-attach once node is ready (no user action)
    if (offlineMode) {
        offlineMode = false;
        statusLine = "Node online — connected";
    }

    // Hold Ready on splash briefly so the checklist is readable, then open shell
    // Do NOT load full RPC help catalog here — that was the post-splash freeze.
    if (readySinceTime <= 0.0)
        readySinceTime = now;
    if (showBootSplash && (now - readySinceTime) > 0.85) {
        showBootSplash = false;
        offlineMode = false;
        readySinceTime = 0.0;
    }
}

static void DrawCheckList(const std::vector<CheckItem>& items)
{
    for (const auto& c : items) {
        ImVec4 col(0.55f, 0.55f, 0.55f, 1.f);
        const char* mark = "[ ]";
        switch (c.state) {
        case CheckItem::State::Pending:
            mark = "[ ]";
            col = ImVec4(0.5f, 0.5f, 0.55f, 1.f);
            break;
        case CheckItem::State::Active:
            mark = "[…]";
            col = ImVec4(1.f, 0.85f, 0.25f, 1.f);
            break;
        case CheckItem::State::Done:
            mark = "[✓]";
            col = ImVec4(0.35f, 0.9f, 0.4f, 1.f);
            break;
        case CheckItem::State::Fail:
            mark = "[✗]";
            col = ImVec4(1.f, 0.35f, 0.3f, 1.f);
            break;
        case CheckItem::State::Skip:
            mark = "[–]";
            col = ImVec4(0.55f, 0.55f, 0.6f, 1.f);
            break;
        }
        ImGui::TextColored(col, "%s  %s", mark, c.label.c_str());
    }
}

void App::DrawBootSplash()
{
    TickBoot();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + kCaptionH));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - kCaptionH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##BootSplash", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoDocking);

    ImGui::Dummy(ImVec2(0, vp->WorkSize.y * 0.06f));
    if (splash.ok()) {
        float h = 160.0f;
        float aspect = splash.width / (float)(splash.height > 0 ? splash.height : 1);
        float w = h * aspect;
        ImGui::SetCursorPosX((vp->WorkSize.x - w) * 0.5f);
        ImGui::Image((ImTextureID)(intptr_t)splash.id, ImVec2(w, h));
    }
    const char* title = "Dogecoin Core Pro";
    ImGui::SetCursorPosX((vp->WorkSize.x - ImGui::CalcTextSize(title).x) * 0.5f);
    ImGui::TextUnformatted(title);
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.85f, 0.25f, 1.f));
    ImGui::SetCursorPosX((vp->WorkSize.x - ImGui::CalcTextSize(bootHeadline.c_str()).x) * 0.5f);
    ImGui::TextUnformatted(bootHeadline.c_str());
    ImGui::PopStyleColor();
    if (!bootDetail.empty()) {
        ImGui::SetCursorPosX((vp->WorkSize.x - ImGui::CalcTextSize(bootDetail.c_str()).x) * 0.5f);
        ImGui::TextDisabled("%s", bootDetail.c_str());
    }
    ImGui::Dummy(ImVec2(0, 12));

    if (NeedsFirstRunPrompt()) {
        cfg.firstRunDone = false;
        const float formW = 560.0f;
        ImGui::SetCursorPosX((vp->WorkSize.x - formW) * 0.5f);
        ImGui::BeginChild("##firstrun", ImVec2(formW, 360), true);
        ImGui::TextWrapped("Welcome to Dogecoin Core.");
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::TextWrapped("As this is the first time the program is launched, you can choose where Dogecoin Core will store its data.");
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextWrapped("Dogecoin Core will download and store a copy of the Dogecoin block chain. At least 57 GB of data will be stored in this directory, and it will grow over time. The wallet will also be stored in this directory.");
        ImGui::TextDisabled("Local disk only — not OneDrive, Drive, iCloud, Dropbox, Proton, pCloud, MEGA, or Nextcloud.");
        ImGui::Dummy(ImVec2(0, 8));
        if (ImGui::RadioButton("Use the default data directory", !useCustomDatadir)) {
            useCustomDatadir = false;
            firstRunError.clear();
            if (defaultDatadir[0])
                std::snprintf(datadirHint, sizeof(datadirHint), "%s", defaultDatadir);
        }
        if (!useCustomDatadir && defaultDatadir[0]) {
            ImGui::Indent(24.0f);
            ImGui::TextDisabled("%s", defaultDatadir);
            ImGui::Unindent(24.0f);
        }
        if (ImGui::RadioButton("Use a custom data directory:", useCustomDatadir)) {
            useCustomDatadir = true;
            firstRunError.clear();
        }
        ImGui::BeginDisabled(!useCustomDatadir);
        ImGui::SetNextItemWidth(formW - 80.0f);
        ImGui::InputText("##datadir", datadirHint, sizeof(datadirHint));
        ImGui::SameLine();
#if defined(_WIN32)
        if (ImGui::Button("...") && useCustomDatadir) {
            char picked[512];
            std::snprintf(picked, sizeof(picked), "%s", datadirHint);
            if (PickFolder(picked, sizeof(picked))) {
                std::snprintf(datadirHint, sizeof(datadirHint), "%s", picked);
                firstRunError.clear();
            }
        }
#else
        ImGui::TextDisabled("...");
#endif
        ImGui::EndDisabled();
        if (datadirHint[0] && DirectoryExists(datadirHint)) {
            if (WalletExistsIn(datadirHint))
                ImGui::TextDisabled("This folder already exists and contains a wallet.dat.");
            else
                ImGui::TextDisabled("Directory already exists. A new wallet.dat will be created here unless you copy a backup in after first start.");
        } else {
            ImGui::TextDisabled("A new data directory will be created.");
        }
        if (LooksLikeConsumerCloudPath(useCustomDatadir ? datadirHint : defaultDatadir)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.25f, 0.2f, 1.f));
            ImGui::TextWrapped("This path is on a consumer cloud sync folder. Pick a local disk.");
            ImGui::PopStyleColor();
        } else if (LooksLikeOperatorFileStore(useCustomDatadir ? datadirHint : defaultDatadir)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.75f, 0.25f, 1.f));
            ImGui::TextWrapped("This looks like a server network disk (virtiofs / NFS). Fine for archives on a VPS — not the live desktop datadir.");
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Separator();
        ImGui::TextUnformatted("Initial sync (Core Pro)");
        if (ImGui::RadioButton("Fast path (recommended) — prune + attested UTXO snapshot from CDN", cfg.preferFastSync)) {
            cfg.preferFastSync = true;
            cfg.prune = true;
        }
        if (ImGui::RadioButton("Full archive — keep all block files (large disk; for explorers / advanced)", !cfg.preferFastSync)) {
            cfg.preferFastSync = false;
            cfg.prune = false;
        }
        ImGui::TextDisabled("Fast path uses ~5.5 GB of blocks and offers Fast Sync from sync.doge.gopastearth.com.");
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::TextUnformatted("Chainstate engine (this datadir)");
        if (ImGui::RadioButton("LevelDB — compatible default", !cfg.preferMdbx))
            cfg.preferMdbx = false;
        if (ImGui::RadioButton("MDBX — new or wiped datadir only (local KV, not consensus)", cfg.preferMdbx))
            cfg.preferMdbx = true;
        ImGui::TextDisabled(
            "Do not pick MDBX if this folder already has a LevelDB chainstate. "
            "Wipe chainstate/ and blocks/ first (keep a wallet.dat backup).");
        if (!firstRunError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.25f, 0.2f, 1.f));
            ImGui::TextWrapped("%s", firstRunError.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::Dummy(ImVec2(0, 10));
        const float btnW = 120.0f;
        ImGui::SetCursorPosX((vp->WorkSize.x - (btnW * 2 + 12)) * 0.5f);
        if (ImGui::Button("OK", ImVec2(btnW, 0)))
            FinishFirstRunAndStart();
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Cancel", ImVec2(btnW, 0)))
            RequestExit();
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    const float listW = 420.0f;
    ImGui::SetCursorPosX((vp->WorkSize.x - listW) * 0.5f);
    ImGui::BeginChild("##bootlist", ImVec2(listW, 200), true);
    DrawCheckList(bootChecks);
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 12));

    // Auto-start only — no "Start node" button (avoids double-start / user footguns).
    // Retry appears only if auto-start failed.
    bool startFailed = false;
    for (const auto& c : bootChecks) {
        if ((c.id == "start" || c.id == "locate") && c.state == CheckItem::State::Fail)
            startFailed = true;
    }
    const float btnW = 160.0f;
    if (startFailed) {
        ImGui::SetCursorPosX((vp->WorkSize.x - (btnW * 2 + 12)) * 0.5f);
        if (ImGui::Button("Retry start", ImVec2(btnW, 0))) {
            autoStartTried = false;
            InitBootChecklist();
            bootStartTime = ImGui::GetTime() - 1.3; // skip intro beat
            lastProbeTime = 0;
        }
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Continue offline", ImVec2(btnW, 0))) {
            offlineMode = true;
            showBootSplash = false;
            statusLine = "Offline — node will attach when ready";
        }
    } else {
        ImGui::SetCursorPosX((vp->WorkSize.x - btnW) * 0.5f);
        if (ImGui::Button("Continue offline", ImVec2(btnW, 0))) {
            offlineMode = true;
            showBootSplash = false;
            statusLine = "Offline — node will attach when ready";
        }
        ImGui::SetCursorPosX((vp->WorkSize.x - ImGui::CalcTextSize("Node starts automatically").x) * 0.5f);
        ImGui::TextDisabled("Node starts automatically");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void App::DrawShutdownSplash()
{
    TickShutdown();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + kCaptionH));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - kCaptionH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##ShutdownSplash", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoDocking);
    ImGui::Dummy(ImVec2(0, vp->WorkSize.y * 0.18f));
    if (brandMark.ok()) {
        float s = 72.0f;
        ImGui::SetCursorPosX((vp->WorkSize.x - s) * 0.5f);
        ImGui::Image((ImTextureID)(intptr_t)brandMark.id, ImVec2(s, s));
    }
    ImGui::Dummy(ImVec2(0, 16));
    ImGui::SetCursorPosX((vp->WorkSize.x - ImGui::CalcTextSize(shutdownHeadline.c_str()).x) * 0.5f);
    ImGui::TextUnformatted(shutdownHeadline.c_str());
    if (!shutdownDetail.empty()) {
        ImGui::SetCursorPosX((vp->WorkSize.x - ImGui::CalcTextSize(shutdownDetail.c_str()).x) * 0.5f);
        ImGui::TextDisabled("%s", shutdownDetail.c_str());
    }
    ImGui::Dummy(ImVec2(0, 16));
    const float listW = 420.0f;
    ImGui::SetCursorPosX((vp->WorkSize.x - listW) * 0.5f);
    ImGui::BeginChild("##stoplist", ImVec2(listW, 140), true);
    DrawCheckList(shutdownChecks);
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void App::BeginContentPage(const char* title)
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float menuH = kCaptionH;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + kRailW, vp->WorkPos.y + menuH));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - kRailW, vp->WorkSize.y - menuH));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin(title, nullptr, flags);
}

void App::EndContentPage()
{
    ImGui::End();
    ImGui::PopStyleVar();
}

void App::DrawSidebar()
{
    navHoverLabel.clear();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    auto centerX = [](float itemW) {
        float x = (ImGui::GetWindowWidth() - itemW) * 0.5f;
        if (x < 0.0f)
            x = 0.0f;
        ImGui::SetCursorPosX(x);
    };

    if (brandMark.ok()) {
        const float s = 40.0f;
        centerX(s);
        ImGui::Image((ImTextureID)(intptr_t)brandMark.id, ImVec2(s, s));
        ImGui::Dummy(ImVec2(0, 6));
    }

    for (const auto& n : nav) {
        const bool selected = (activeNav == n.id);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushID(n.id.c_str());
        const float icon = 40.0f;
        centerX(icon);
        bool clicked = n.tex.ok()
                           ? ImGui::ImageButton("##nav", (ImTextureID)(intptr_t)n.tex.id, ImVec2(icon, icon))
                           : ImGui::Button(n.label.c_str(), ImVec2(icon, icon));
        if (ImGui::IsItemHovered()) {
            navHoverLabel = n.label;
            navHoverY = ImGui::GetItemRectMin().y + (ImGui::GetItemRectSize().y * 0.5f);
        }
        if (clicked)
            activeNav = n.id;
        ImGui::PopID();
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar(3);
}

void App::DrawNavFlyout()
{
    if (navHoverLabel.empty())
        return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float pad = 8.0f;
    ImVec2 size = ImGui::CalcTextSize(navHoverLabel.c_str());
    ImVec2 pos(vp->WorkPos.x + kRailW + 6.0f, navHoverY - size.y * 0.5f - pad);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_Tooltip;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::Begin("##NavFlyout", nullptr, flags);
    ImGui::TextUnformatted(navHoverLabel.c_str());
    ImGui::End();
    ImGui::PopStyleVar(2);
}

static std::vector<std::string> SplitCsv(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty())
                out.push_back(cur);
            cur.clear();
        } else if (c != ' ') {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

void App::EnsureHomePanels()
{
    homePanels = SplitCsv(cfg.homePanels);
    if (homePanels.empty()) {
        homePanels = {"balance", "sync", "peers", "actions"};
        PersistHomePanels();
    }
}

void App::PersistHomePanels()
{
    cfg.homePanels.clear();
    for (size_t i = 0; i < homePanels.size(); ++i) {
        if (i)
            cfg.homePanels += ",";
        cfg.homePanels += homePanels[i];
    }
}

void App::AddHomePanel(const std::string& id)
{
    for (const auto& p : homePanels)
        if (p == id)
            return;
    homePanels.push_back(id);
    PersistHomePanels();
    SaveSettings(settingsPath, cfg);
}

void App::RemoveHomePanel(int index)
{
    if (index < 0 || index >= (int)homePanels.size())
        return;
    homePanels.erase(homePanels.begin() + index);
    PersistHomePanels();
    SaveSettings(settingsPath, cfg);
}

void App::MoveHomePanel(int index, int delta)
{
    int n = (int)homePanels.size();
    int dest = index + delta;
    if (index < 0 || dest < 0 || index >= n || dest >= n)
        return;
    std::swap(homePanels[index], homePanels[dest]);
    PersistHomePanels();
    SaveSettings(settingsPath, cfg);
}

void App::DrawHomePanel(const std::string& id, int index)
{
    ImGui::PushID(index);
    ImGui::BeginChild(id.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(id == "balance" ? "Balance"
                           : id == "sync"  ? "Chain / sync"
                           : id == "peers" ? "Network"
                           : id == "actions" ? "Quick actions"
                           : id == "arcade" ? "Arcade"
                           : id == "mining" ? "Mining"
                                           : id.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 92);
    if (ImGui::SmallButton("^") && index > 0)
        MoveHomePanel(index, -1);
    ImGui::SameLine();
    if (ImGui::SmallButton("v"))
        MoveHomePanel(index, 1);
    ImGui::SameLine();
    if (ImGui::SmallButton("x"))
        RemoveHomePanel(index);
    ImGui::Separator();

    if (id == "balance") {
        if (icons.count("balance") && icons["balance"].ok()) {
            ImGui::Image((ImTextureID)(intptr_t)icons["balance"].id, ImVec2(28, 28));
            ImGui::SameLine();
        }
        if (snap.hasWallet)
            ImGui::Text("%.8f %s", snap.balance, displayUnit);
        if (snap.hasWallet && !snap.walletEncrypted)
            ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f),
                               "Wallet is not encrypted — Options → Wallet");
        else
            ImGui::TextUnformatted("--");
        if (snap.hasWallet && snap.unconfirmed != 0.0)
            ImGui::TextDisabled("unconfirmed %.8f", snap.unconfirmed);
    } else if (id == "sync") {
        if (snap.connected)
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.f), "Connected");
        else
            ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "Waiting for node");
        if (snap.stale) {
            ImGui::SameLine();
            ImGui::TextDisabled("updating…");
        }
        ImGui::SetWindowFontScale(1.2f);
        DrawBlocksHeadersLine("Blocks ", DisplayBlocks(snap, cfg.lastKnownHeight),
                              DisplayHeaders(snap, cfg.lastKnownHeaders));
        ImGui::SetWindowFontScale(1.0f);
        float prog = (float)snap.verificationProgress;
        if (prog < 0)
            prog = 0;
        if (prog > 1)
            prog = 1;
        char ov[32];
        std::snprintf(ov, sizeof(ov), "%.1f%%", prog * 100.0);
        ImGui::ProgressBar(prog, ImVec2(-1, 0), ov);
        DrawDbcacheGauge(snap);
        if (snap.initialBlockDownload) {
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.25f, 1.f), "Initial block download — resumes after a clean Exit");
            if (cfg.lastKnownHeight > 0 && snap.blocks + 50 < cfg.lastKnownHeight)
                ImGui::TextWrapped(
                    "Tip is below the last saved height (%d). A force-kill mid-flush "
                    "can do that. Let this session run; File → Exit waits for flush.",
                    cfg.lastKnownHeight);
        } else if (snap.connected)
            ImGui::TextDisabled("Tip in sync");
        if (!snap.dbEngine.empty())
            ImGui::TextDisabled("KV engine: %s", snap.dbEngine.c_str());
        if (ImGui::SmallButton("Open Chain / Fast Sync"))
            activeNav = "blocks";
    } else if (id == "peers") {
        ImGui::Text("Peers: %d", snap.peerCount);
        if (ImGui::Button("Open Network map"))
            activeNav = "network";
    } else if (id == "actions") {
        if (ImGui::Button("Send")) {
            activeNav = "send";
            sendReceiveTab = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Receive")) {
            activeNav = "send";
            sendReceiveTab = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("History"))
            activeNav = "tx";
        ImGui::SameLine();
        if (ImGui::Button("Arcade"))
            activeNav = "arcade";
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            lastProbeTime = 0;
            TickNodeProbe();
        }
    } else if (id == "arcade") {
        ImGui::TextDisabled("GPE cabinet — unlocked in Core Pro");
        if (ImGui::Button("Open Arcade"))
            activeNav = "arcade";
    } else if (id == "mining") {
        if (snap.hasMining)
            ImGui::Text("See Mining page for hashrate / network");
        else
            ImGui::TextDisabled("getmininginfo not available yet");
        if (ImGui::Button("Open Mining"))
            activeNav = "mining";
    } else if (id == "memestream") {
        DrawMemeRail();
    }
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::Spacing();
}

void App::DrawHome()
{
    BeginContentPage("Overview");
    if (splash.ok()) {
        float h = 72.0f;
        float aspect = splash.width / (float)(splash.height > 0 ? splash.height : 1);
        ImGui::Image((ImTextureID)(intptr_t)splash.id, ImVec2(h * aspect, h));
        ImGui::SameLine();
    }
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Dogecoin Core Pro");
    if (IsTestnet())
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.35f, 1.f), "TESTNET");
    if (snap.connected)
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.f), "Connected");
    else if (offlineMode)
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.f), "Offline");
    else
        ImGui::Text("Disconnected");
    ImGui::EndGroup();
    ImGui::Separator();

    EnsureHomePanels();
    memeWebOnHome = false;
    const float avail = ImGui::GetContentRegionAvail().x;
    const bool wide = avail > 920.0f;
    if (wide) {
        ImGui::BeginChild("##homeleft", ImVec2(avail - 360.0f, 0), false);
        for (int i = 0; i < (int)homePanels.size(); ++i) {
            if (homePanels[i] == "memestream")
                continue;
            DrawHomePanel(homePanels[i], i);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##homerail", ImVec2(348.0f, 0), true);
        DrawMemeRail();
        ImGui::EndChild();
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        memeWebX = (int)(a.x - vp->Pos.x) + 8;
        memeWebY = (int)(a.y - vp->Pos.y) + 36;
        memeWebW = (int)(b.x - a.x) - 16;
        memeWebH = (int)(b.y - a.y) - 44;
        memeWebOnHome = memeWebW > 40 && memeWebH > 80;
    } else {
        for (int i = 0; i < (int)homePanels.size(); ++i) {
            if (homePanels[i] == "memestream")
                continue;
            DrawHomePanel(homePanels[i], i);
        }
        ImGui::Spacing();
        if (ImGui::Button("Open Meme Stream", ImVec2(220, 0)))
            activeNav = "meme";
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Customize home");
    const char* addIds[] = {"balance", "sync", "peers", "actions", "arcade", "mining"};
    const char* addLabels[] = {"Balance", "Chain / sync", "Network", "Quick actions", "Arcade", "Mining"};
    for (int i = 0; i < 6; ++i) {
        bool have = false;
        for (const auto& p : homePanels)
            if (p == addIds[i])
                have = true;
        if (have)
            continue;
        if (ImGui::SmallButton(addLabels[i]))
            AddHomePanel(addIds[i]);
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::Separator();
    if (ImGui::Button("Send / Receive")) {
        activeNav = "send";
        sendReceiveTab = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Transactions")) {
        activeNav = "tx";
        historyLoaded = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Arcade"))
        OpenArcadeHub();
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        lastProbeTime = 0;
        TickNodeProbe();
    }
    EndContentPage();
}

// Stable pseudo-geo from peer address until a real GeoIP DB is wired.
// Prefer IPv4 first-octet regional bias so dots cluster by rough RIR space.
static void PeerPseudoLatLon(const std::string& addr, float& lat, float& lon)
{
    unsigned h = 2166136261u;
    for (unsigned char c : addr) {
        h ^= c;
        h *= 16777619u;
    }
    // Try parse a.b.c.d for coarse region bias
    int a = -1, b = 0, c = 0, d = 0;
    if (std::sscanf(addr.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) >= 1 && a >= 0) {
        // Very rough public IP geography heuristics (not accurate — display only)
        if (a >= 1 && a <= 50) {          // often Americas-ish
            lat = 15.0f + (float)(h % 5000) / 100.0f;   // ~15..65
            lon = -130.0f + (float)((h / 7) % 8000) / 100.0f; // Americas band
            if (a >= 40) lon = -80.0f + (float)((h / 11) % 4000) / 100.0f;
        } else if (a >= 51 && a <= 100) { // Europe / Africa-ish
            lat = 30.0f + (float)(h % 3500) / 100.0f;
            lon = -20.0f + (float)((h / 5) % 7000) / 100.0f;
        } else if (a >= 101 && a <= 150) { // Asia / Pacific-ish
            lat = 10.0f + (float)(h % 4500) / 100.0f;
            lon = 60.0f + (float)((h / 3) % 9000) / 100.0f;
        } else if (a >= 151 && a <= 200) {
            lat = -20.0f + (float)(h % 4000) / 100.0f;
            lon = -70.0f + (float)((h / 9) % 5000) / 100.0f;
        } else {
            lat = ((h % 14000) / 100.0f) - 70.0f;
            lon = (((h / 14000) % 36000) / 100.0f) - 180.0f;
        }
        // Jitter so peers in same /8 don't stack
        lat += ((h % 17) - 8) * 0.35f;
        lon += (((h / 17) % 17) - 8) * 0.45f;
    } else {
        lat = ((h % 14000) / 100.0f) - 70.0f;
        lon = (((h / 14000) % 36000) / 100.0f) - 180.0f;
    }
    if (lat > 85.f) lat = 85.f;
    if (lat < -85.f) lat = -85.f;
    if (lon > 179.f) lon = 179.f;
    if (lon < -179.f) lon = -179.f;
}

void App::DrawWorldPeerMap(float width, float height)
{
    // Prefer filling the requested height (big map); only shrink if extremely tall
    if (worldMap.ok() && worldMap.height > 0 && width > 1.0f) {
        float aspect = worldMap.width / (float)worldMap.height;
        float byAspect = width / aspect;
        // Use the larger of requested height and natural aspect height, capped
        if (byAspect > height)
            height = byAspect;
    }
    if (height < 200.0f)
        height = 200.0f;
    if (height > 900.0f)
        height = 900.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p1(p0.x + width, p0.y + height);

    // Dark plate behind map
    dl->AddRectFilled(p0, p1, IM_COL32(8, 10, 16, 255), 6.0f);

    if (worldMap.ok()) {
        // globaltransparent equirectangular: white land / black ocean — gold-tint land slightly
        ImGui::SetCursorScreenPos(p0);
        ImGui::Image((ImTextureID)(intptr_t)worldMap.id, ImVec2(width, height),
                     ImVec2(0, 0), ImVec2(1, 1),
                     ImVec4(0.92f, 0.88f, 0.72f, 1.0f), // soft gold land
                     ImVec4(0, 0, 0, 0));
    } else {
        ImGui::SetCursorScreenPos(p0);
        ImGui::Dummy(ImVec2(width, height));
        dl->AddText(ImVec2(p0.x + 12, p0.y + 12), IM_COL32(200, 160, 60, 255),
                    "worldMap.png missing (assets/network/worldMap.png)");
    }

    dl->AddRect(p0, p1, IM_COL32(180, 140, 40, 180), 6.0f, 0, 1.5f);

    // Equirectangular: x = lon, y = lat (standard for this basemap)
    auto project = [&](float lat, float lon) -> ImVec2 {
        float x = p0.x + (lon + 180.0f) / 360.0f * width;
        float y = p0.y + (90.0f - lat) / 180.0f * height;
        return ImVec2(x, y);
    };

    for (const auto& pr : snap.peers) {
        float lat = 0, lon = 0;
        PeerPseudoLatLon(pr.addr, lat, lon);
        ImVec2 pt = project(lat, lon);
        ImU32 col = pr.inbound ? IM_COL32(80, 200, 255, 255) : IM_COL32(255, 196, 48, 255);
        // Glow + core so dots read on white land
        dl->AddCircleFilled(pt, 7.0f, IM_COL32(0, 0, 0, 140));
        dl->AddCircleFilled(pt, 4.5f, col);
        dl->AddCircle(pt, 5.5f, IM_COL32(255, 255, 255, 90), 0, 1.0f);
        if (ImGui::IsMouseHoveringRect(ImVec2(pt.x - 9, pt.y - 9), ImVec2(pt.x + 9, pt.y + 9))) {
            ImGui::BeginTooltip();
            ImGui::Text("%s  %s", pr.inbound ? "IN" : "OUT", pr.addr.c_str());
            if (!pr.subver.empty())
                ImGui::TextDisabled("%s", pr.subver.c_str());
            ImGui::Text("ping %.0f ms · height %d", pr.ping * 1000.0, pr.startingheight);
            ImGui::TextDisabled("Position approximate (no GeoIP yet)");
            ImGui::EndTooltip();
        }
    }

    // Legend over map corner
    dl->AddRectFilled(ImVec2(p0.x + 8, p1.y - 32), ImVec2(p0.x + 210, p1.y - 8),
                      IM_COL32(0, 0, 0, 160), 4.0f);
    dl->AddCircleFilled(ImVec2(p0.x + 22, p1.y - 20), 4.0f, IM_COL32(255, 196, 48, 255));
    dl->AddText(ImVec2(p0.x + 32, p1.y - 28), IM_COL32(230, 230, 230, 255), "outbound");
    dl->AddCircleFilled(ImVec2(p0.x + 120, p1.y - 20), 4.0f, IM_COL32(80, 200, 255, 255));
    dl->AddText(ImVec2(p0.x + 130, p1.y - 28), IM_COL32(230, 230, 230, 255), "inbound");

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + 4));
    ImGui::Dummy(ImVec2(width, 2));
    ImGui::TextDisabled("Basemap: assets/network/worldMap.png · peer pins approximate until GeoIP");
}

void App::DrawNetwork()
{
    BeginContentPage("Network");
    if (!snap.connected) {
        ImGui::TextDisabled("Not connected — node attaches automatically when ready.");
        EndContentPage();
        return;
    }

    if (icons.count("peers_map") && icons["peers_map"].ok()) {
        ImGui::Image((ImTextureID)(intptr_t)icons["peers_map"].id, ImVec2(28, 28));
        ImGui::SameLine();
    }
    ImGui::BeginGroup();
    ImGui::Text("Version %s  (%d)", snap.subversion.c_str(), snap.version);
    ImGui::Text("Connections: %d   ·   network active: %s", snap.connections,
                snap.networkActive.empty() ? "?" : snap.networkActive.c_str());
    ImGui::TextDisabled("P2P proxy: %s",
                        snap.p2pProxy.empty() ? "none (clearnet)" : snap.p2pProxy.c_str());
    ImGui::EndGroup();

    ImGui::Separator();
    // Big map, compact scrollable peer list
    const float peerTableH = 140.0f;
    float mapW = ImGui::GetContentRegionAvail().x;
    float mapH = ImGui::GetContentRegionAvail().y - peerTableH - 48.0f;
    if (mapW < 200) mapW = 200;
    if (mapH < 280.0f) mapH = 280.0f;
    DrawWorldPeerMap(mapW, mapH);

    ImGui::Separator();
    ImGui::TextUnformatted("Peers");
    if (ImGui::BeginTable("peers", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, peerTableH))) {
        ImGui::TableSetupColumn("Dir", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Height", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Synced", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Agent");
        ImGui::TableHeadersRow();
        for (const auto& pr : snap.peers) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(pr.inbound ? "in" : "out");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(pr.addr.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.0f ms", pr.ping * 1000.0);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", pr.startingheight);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", pr.synced_blocks >= 0 ? pr.synced_blocks : pr.synced_headers);
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(pr.subver.c_str());
        }
        ImGui::EndTable();
    }
    EndContentPage();
}

void App::DrawChain()
{
    BeginContentPage("Chain / Fast Sync");
    if (!snap.connected) {
        ImGui::TextDisabled("Start dogecoind for chain status.");
        EndContentPage();
        return;
    }
    ImGui::Text("Best: %s", snap.bestBlockHash.empty() ? "--" : snap.bestBlockHash.c_str());
    DrawBlocksHeadersLine("Height:", DisplayBlocks(snap, cfg.lastKnownHeight),
                          DisplayHeaders(snap, cfg.lastKnownHeaders));
    if (snap.stale)
        ImGui::TextDisabled("RPC busy — last height held (not a rewind).");
    float prog = (float)snap.verificationProgress;
    if (prog < 0)
        prog = 0;
    if (prog > 1)
        prog = 1;
    char ov[32];
    std::snprintf(ov, sizeof(ov), "%.1f%%", prog * 100.0);
    ImGui::ProgressBar(prog, ImVec2(-1, 0), ov);
    DrawDbcacheGauge(snap);
    if (snap.initialBlockDownload)
        ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f), "initialblockdownload = true");
    if (snap.hasIbdInfo)
        ImGui::TextWrapped("%s", snap.ibdSummary.c_str());

    ImGui::Separator();
    ImGui::TextUnformatted("AssumeUTXO / Fast Sync (node-side RPC)");
    ImGui::Text("validated=%s  failed=%s  dual_collapsed=%s  progress=%.2f",
                snap.assumeUtxoValidated ? "true" : "false",
                snap.assumeUtxoFailed ? "true" : "false",
                snap.assumeUtxoDualCollapsed ? "true" : "false",
                snap.assumeUtxoProgress);
    if (snap.snapshotChainstateActive)
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.f, 1.f), "Snapshot chainstate active");

    ImGui::TextDisabled("Official GPE CDN - leave these as-is for the default.");
    ImGui::TextUnformatted("Manifest URL");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##manifest", snapshotManifest, sizeof(snapshotManifest));
    ImGui::TextUnformatted("Local snapshot path");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##snappath", snapshotPath, sizeof(snapshotPath));
    ImGui::TextUnformatted("Artifact SHA-256");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##snapsha", snapshotShaArg, sizeof(snapshotShaArg));
    if (!fastSyncArtifactUrl.empty())
        ImGui::TextWrapped("Artifact: %s", fastSyncArtifactUrl.c_str());
    if (!fastSyncHeight.empty())
        ImGui::Text("Attested height %s   size %s bytes", fastSyncHeight.c_str(),
                    fastSyncBytes.empty() ? "?" : fastSyncBytes.c_str());

    const bool busy = fastSyncBusy.load();
    if (busy)
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.25f, 1.f), "Working... window stays responsive.");

    if (ImGui::Button("listassumeutxo")) {
        auto r = rpc.call("listassumeutxo");
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = r.ok ? r.resultJson : r.error;
    }
    ImGui::SameLine();
    if (ImGui::Button("getibdinfo")) {
        auto r = rpc.call("getibdinfo");
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = r.ok ? r.resultJson : r.error;
        lastProbeTime = 0;
        TickNodeProbe();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("Resolve manifest (JSON only)"))
        StartResolveManifest();
    ImGui::SameLine();
    if (ImGui::Button("Download snapshot (~11 GB)"))
        StartDownloadSnapshot();
    ImGui::EndDisabled();

    ImGui::BeginDisabled(busy || !snapshotPath[0]);
    if (ImGui::Button("Load snapshot (no activate)"))
        StartLoadSnapshot(false);
    ImGui::SameLine();
    if (ImGui::Button("Load + activate"))
        StartLoadSnapshot(true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("activatesnapshot") && !busy) {
        auto r = rpc.call("activatesnapshot");
        std::lock_guard<std::mutex> lock(fastSyncMu);
        fastSyncStatus = r.ok ? r.resultJson : (r.error + "\n" + r.body);
        lastProbeTime = 0;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Cold offload (optional)");
    ImGui::TextUnformatted("Archive path (finalized blocks only)");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##archivepath", archivePath, sizeof(archivePath));
    ImGui::TextDisabled("Desktop: local or cloud folder. Node copies here before prune deletes local blk/rev.");
    ImGui::BeginDisabled(busy || !snap.connected);
    if (ImGui::Button("Archive status"))
        StartArchiveRpc("getarchiveinfo");
    ImGui::SameLine();
    if (ImGui::Button("Verify archive hashes"))
        StartArchiveRpc("verifyarchive");
    ImGui::EndDisabled();
    if (ImGui::CollapsingHeader("Server / operator only")) {
        ImGui::TextUnformatted("Snapshot dest (dumps / CDN origin)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##snapdest", snapshotDestPath, sizeof(snapshotDestPath));
        ImGui::TextDisabled("Headless box only (Vultr File System, NFS, extra disk). Never the live datadir.");
        ImGui::BeginDisabled(busy || !snap.connected || !snapshotDestPath[0]);
        if (ImGui::Button("Dump UTXO set to snapshot dest")) {
#if defined(_WIN32)
            const std::string dest = std::string(snapshotDestPath) + "\\utxo_dump.dat";
#else
            const std::string dest = std::string(snapshotDestPath) + "/utxo_dump.dat";
#endif
            StartArchiveRpc("dumptxoutset", "[\"" + JsonEscape(dest) + "\"]");
        }
        ImGui::EndDisabled();
    }

    ImGui::TextWrapped(
        "Resolve only pulls latest.json (small). Download is the multi-GB artifact "
        "(that used to freeze this window because it ran on the UI thread). "
        "Prefer a fresh datadir. Mid-IBD + prune past ~10k blocks is refused. "
        "Consensus stays in dogecoind.");
    std::string statusCopy;
    {
        std::lock_guard<std::mutex> lock(fastSyncMu);
        statusCopy = fastSyncStatus;
    }
    if (!statusCopy.empty()) {
        ImGui::Separator();
        ImGui::BeginChild("##fsout", ImVec2(0, 160), true);
        ImGui::TextWrapped("%s", statusCopy.c_str());
        ImGui::EndChild();
    }
    EndContentPage();
}

static std::string FormatHashrate(double h)
{
    char buf[64];
    if (h >= 1e18) std::snprintf(buf, sizeof(buf), "%.2f EH/s", h / 1e18);
    else if (h >= 1e15) std::snprintf(buf, sizeof(buf), "%.2f PH/s", h / 1e15);
    else if (h >= 1e12) std::snprintf(buf, sizeof(buf), "%.2f TH/s", h / 1e12);
    else if (h >= 1e9) std::snprintf(buf, sizeof(buf), "%.2f GH/s", h / 1e9);
    else if (h >= 1e6) std::snprintf(buf, sizeof(buf), "%.2f MH/s", h / 1e6);
    else if (h >= 1e3) std::snprintf(buf, sizeof(buf), "%.2f kH/s", h / 1e3);
    else std::snprintf(buf, sizeof(buf), "%.2f H/s", h);
    return buf;
}

static std::string FormatTime(long long t)
{
    if (t <= 0) return "--";
#if defined(_WIN32)
    time_t tt = (time_t)t;
    struct tm tm {};
    localtime_s(&tm, &tt);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
#else
    time_t tt = (time_t)t;
    struct tm tm {};
    localtime_r(&tt, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
#endif
}

void App::ParseHistoryJson(const std::string& json)
{
    historyRows.clear();
    if (json.empty() || json[0] != '[') return;
    size_t i = 0;
    while (i < json.size()) {
        auto o = json.find('{', i);
        if (o == std::string::npos) break;
        int depth = 0;
        size_t k = o;
        for (; k < json.size(); ++k) {
            if (json[k] == '{') depth++;
            else if (json[k] == '}') {
                depth--;
                if (depth == 0) {
                    k++;
                    break;
                }
            }
        }
        std::string obj = json.substr(o, k - o);
        TxRow row;
        row.category = RpcClient::jsonString(obj, "category");
        row.address = RpcClient::jsonString(obj, "address");
        row.label = RpcClient::jsonString(obj, "label");
        row.txid = RpcClient::jsonString(obj, "txid");
        row.amount = RpcClient::jsonDouble(obj, "amount");
        row.confirmations = RpcClient::jsonInt(obj, "confirmations", 0);
        row.time = (long long)RpcClient::jsonDouble(obj, "time");
        if (row.time <= 0)
            row.time = (long long)RpcClient::jsonDouble(obj, "timereceived");
        if (!row.txid.empty() || !row.category.empty())
            historyRows.push_back(row);
        i = k;
        if (historyRows.size() >= 200) break;
    }
    // Newest first
    std::reverse(historyRows.begin(), historyRows.end());
}

void App::RefreshHistory()
{
    if (!snap.connected || !snap.hasWallet) {
        historyStatus = "Wallet not available";
        historyRows.clear();
        historyLoaded = true;
        return;
    }
    auto r = rpc.call("listtransactions", "[\"*\",50]");
    lastHistoryFetch = ImGui::GetTime();
    if (!r.ok) {
        historyStatus = r.error;
        historyLoaded = true;
        return;
    }
    ParseHistoryJson(r.resultJson);
    historyStatus = std::to_string(historyRows.size()) + " transactions";
    historyLoaded = true;
}

void App::DrawSendReceive()
{
    BeginContentPage("Send / Receive");
    if (!snap.connected) {
        ImGui::TextDisabled("Not connected — waiting for node…");
        EndContentPage();
        return;
    }

    if (snap.hasWallet)
        ImGui::Text("Available: %.8f %s   (unconfirmed %.8f)", snap.balance, displayUnit, snap.unconfirmed);
    else
        ImGui::TextDisabled("Wallet RPC not available.");

    if (ImGui::BeginTabBar("##sr")) {
        if (ImGui::BeginTabItem("Send")) {
            sendReceiveTab = 0;
            if (icons.count("send") && icons["send"].ok()) {
                ImGui::Image((ImTextureID)(intptr_t)icons["send"].id, ImVec2(32, 32));
                ImGui::SameLine();
            }
            ImGui::TextUnformatted("Send DOGE");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("Pay to", sendAddr, sizeof(sendAddr));
            ImGui::SetNextItemWidth(220);
            ImGui::InputText("Amount", sendAmount, sizeof(sendAmount));
            ImGui::SameLine();
            ImGui::TextUnformatted(displayUnit);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("Comment (optional)", sendComment, sizeof(sendComment));
            ImGui::Spacing();
            const bool canSend = snap.hasWallet && sendAddr[0] && sendAmount[0];
            if (!canSend)
                ImGui::BeginDisabled();
            if (ImGui::Button("Send payment", ImVec2(160, 0))) {
                std::ostringstream p;
                // sendtoaddress "addr" amount "comment"
                p << "[\"" << sendAddr << "\"," << sendAmount;
                if (sendComment[0])
                    p << ",\"" << sendComment << "\"";
                p << "]";
                auto r = rpc.call("sendtoaddress", p.str());
                if (r.ok) {
                    sendStatus = "Sent — txid " + r.resultJson;
                    historyLoaded = false; // refresh list next visit
                    lastProbeTime = 0;
                } else {
                    sendStatus = "Failed: " + r.error;
                }
                statusLine = sendStatus;
            }
            if (!canSend)
                ImGui::EndDisabled();
            if (!sendStatus.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", sendStatus.c_str());
            }
            ImGui::TextDisabled("Fees and coin selection are handled by dogecoind.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Receive")) {
            sendReceiveTab = 1;
            if (icons.count("receive") && icons["receive"].ok()) {
                ImGui::Image((ImTextureID)(intptr_t)icons["receive"].id, ImVec2(32, 32));
                ImGui::SameLine();
            }
            ImGui::TextUnformatted("Receive DOGE");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(280);
            ImGui::InputText("Label (optional)", receiveLabel, sizeof(receiveLabel));
            if (ImGui::Button("Request new address") && snap.hasWallet) {
                std::string params = "[]";
                if (receiveLabel[0])
                    params = "[\"" + std::string(receiveLabel) + "\"]";
                auto r = rpc.call("getnewaddress", params);
                if (r.ok) {
                    std::string a = r.resultJson;
                    if (a.size() >= 2 && a.front() == '"')
                        a = a.substr(1, a.size() - 2);
                    std::snprintf(receiveAddr, sizeof(receiveAddr), "%s", a.c_str());
                    receiveStatus = "New address ready — share it to receive payments.";
                } else {
                    receiveStatus = r.error;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Change address") && snap.hasWallet) {
                auto r = rpc.call("getrawchangeaddress");
                if (r.ok) {
                    std::string a = r.resultJson;
                    if (a.size() >= 2 && a.front() == '"')
                        a = a.substr(1, a.size() - 2);
                    std::snprintf(receiveAddr, sizeof(receiveAddr), "%s", a.c_str());
                    receiveStatus = "Change address (internal use).";
                } else {
                    receiveStatus = r.error;
                }
            }
            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("Address", receiveAddr, sizeof(receiveAddr), ImGuiInputTextFlags_ReadOnly);
            if (ImGui::Button("Copy address") && receiveAddr[0])
                CopyToClipboard(receiveAddr);
            ImGui::SameLine();
            if (ImGui::Button("Copy dogecoin: URI") && receiveAddr[0]) {
                CopyToClipboard(std::string("dogecoin:") + receiveAddr);
                receiveStatus = "URI copied";
            }
            if (receiveAddr[0]) {
                ImGui::Dummy(ImVec2(0, 10));
                DrawQrCode(std::string("dogecoin:") + receiveAddr, 180.0f);
            }
            if (!receiveStatus.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", receiveStatus.c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    EndContentPage();
}

void App::DrawMining()
{
    BeginContentPage("Mining");
    if (!snap.connected) {
        ImGui::TextDisabled("Not connected.");
        EndContentPage();
        return;
    }
    if (!snap.hasMining) {
        ImGui::TextDisabled("getmininginfo unavailable.");
        if (ImGui::Button("Retry")) {
            lastProbeTime = 0;
            TickNodeProbe();
        }
        EndContentPage();
        return;
    }

    ImGui::TextUnformatted("Network mining overview");
    ImGui::TextDisabled("This is getmininginfo from dogecoind — not a built-in miner GUI.");
    ImGui::Separator();

    // Stat cards in a row
    auto card = [&](const char* title, const char* value) {
        ImGui::BeginChild(title, ImVec2(180, 72), true);
        ImGui::TextDisabled("%s", title);
        ImGui::TextUnformatted(value);
        ImGui::EndChild();
    };
    char b1[32], b2[48], b3[48], b4[32];
    std::snprintf(b1, sizeof(b1), "%d", snap.miningBlocks);
    std::snprintf(b2, sizeof(b2), "%.4g", snap.difficulty);
    std::string hr = FormatHashrate(snap.networkHashPs);
    std::snprintf(b4, sizeof(b4), "%d", snap.pooledTx);

    card("Height", b1);
    ImGui::SameLine();
    card("Difficulty", b2);
    ImGui::SameLine();
    card("Network hashrate", hr.c_str());
    ImGui::SameLine();
    card("Mempool txs", b4);

    ImGui::Spacing();
    ImGui::Text("Chain: %s", snap.miningChain.empty() ? "--" : snap.miningChain.c_str());
    if (!snap.miningErrors.empty())
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "Node errors: %s", snap.miningErrors.c_str());
    else
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.f), "No mining errors reported");

    ImGui::Separator();
    if (ImGui::Button("Refresh")) {
        lastProbeTime = 0;
        TickNodeProbe();
    }
    ImGui::SameLine();
    if (ImGui::Button("Query networkhashps")) {
        auto r = rpc.call("getnetworkhashps");
        if (r.ok)
            statusLine = "networkhashps " + r.resultJson;
        else
            statusLine = r.error;
    }
    ImGui::TextDisabled("Solo mining: use generate / setgenerate via Console if enabled on this build.");
    EndContentPage();
}

void App::DrawHistory()
{
    BeginContentPage("Transactions");
    if (!snap.connected) {
        ImGui::TextDisabled("Not connected.");
        EndContentPage();
        return;
    }

    // Auto-load once when opening this page
    if ((!historyLoaded || (ImGui::GetTime() - lastHistoryFetch) > 30.0) && snap.hasWallet) {
        RefreshHistory();
    }

    if (ImGui::Button("Refresh"))
        RefreshHistory();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", historyStatus.c_str());

    if (!snap.hasWallet) {
        ImGui::TextDisabled("No wallet loaded.");
        EndContentPage();
        return;
    }

    if (ImGui::BeginTable("tx", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Conf", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 130);
        ImGui::TableSetupColumn("Label / address");
        ImGui::TableSetupColumn("Txid");
        ImGui::TableHeadersRow();
        for (const auto& tx : historyRows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImVec4 col(0.8f, 0.8f, 0.8f, 1.f);
            if (tx.category == "receive") col = ImVec4(0.35f, 0.9f, 0.45f, 1.f);
            else if (tx.category == "send") col = ImVec4(1.f, 0.45f, 0.4f, 1.f);
            else if (tx.category == "generate" || tx.category == "immature")
                col = ImVec4(1.f, 0.85f, 0.3f, 1.f);
            ImGui::TextColored(col, "%s", tx.category.empty() ? "?" : tx.category.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(col, "%s%.8f", tx.amount >= 0 ? "+" : "", tx.amount);

            ImGui::TableSetColumnIndex(2);
            if (tx.confirmations <= 0)
                ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f), "%d", tx.confirmations);
            else
                ImGui::Text("%d", tx.confirmations);

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(FormatTime(tx.time).c_str());

            ImGui::TableSetColumnIndex(4);
            if (!tx.label.empty())
                ImGui::Text("%s", tx.label.c_str());
            else
                ImGui::TextDisabled("%s", tx.address.empty() ? "—" : tx.address.c_str());

            ImGui::TableSetColumnIndex(5);
            std::string shortTx = tx.txid.size() > 16 ? tx.txid.substr(0, 10) + "…" + tx.txid.substr(tx.txid.size() - 6)
                                                      : tx.txid;
            if (ImGui::Selectable(shortTx.c_str()))
                CopyToClipboard(tx.txid);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s\n(click to copy)", tx.txid.c_str());
        }
        ImGui::EndTable();
    }
    EndContentPage();
}

void App::DrawConsole()
{
    BeginContentPage("Console / RPC Help");
    ImGui::TextWrapped(
        "Same command surface as dogecoin-cli / Qt debug console. "
        "Type a method, optional args, or pick from help. Results are delivered below.");

    // Left: command catalog
    ImGui::BeginChild("##helplist", ImVec2(220, 0), true);
    ImGui::TextUnformatted("Commands");
    if (ImGui::Button("Reload help") && snap.connected) {
        helpLoaded = false;
        RefreshHelpCatalog();
    }
    ImGui::InputText("Filter", helpFilter, sizeof(helpFilter));
    if (ImGui::BeginListBox("##cmds", ImVec2(-1, -1))) {
        for (const auto& c : helpCommands) {
            if (helpFilter[0] && c.find(helpFilter) == std::string::npos)
                continue;
            if (ImGui::Selectable(c.c_str())) {
                std::snprintf(consoleInput, sizeof(consoleInput), "%s", c.c_str());
                std::snprintf(consoleParams, sizeof(consoleParams), "[]");
                // auto help detail
                auto r = rpc.call("help", "[\"" + c + "\"]");
                helpDetail = r.ok ? r.resultJson : r.error;
            }
        }
        ImGui::EndListBox();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##conright", ImVec2(0, 0), false);
    ImGui::TextUnformatted("Command help");
    ImGui::BeginChild("##helpdetail", ImVec2(0, 120), true);
    ImGui::TextWrapped("%s", helpDetail.empty() ? "Select a command or run: help" : helpDetail.c_str());
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextUnformatted("Execute");
    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputText("##in", consoleInput, sizeof(consoleInput),
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("params JSON", consoleParams, sizeof(consoleParams));
    if (ImGui::Button("Run") || enter) {
        RunConsoleLine(consoleInput);
    }
    ImGui::SameLine();
    if (ImGui::Button("help")) {
        std::snprintf(consoleInput, sizeof(consoleInput), "help");
        RunConsoleLine("help");
        helpLoaded = false;
        RefreshHelpCatalog();
    }
    ImGui::SameLine();
    if (ImGui::Button("help <cmd>") && consoleInput[0]) {
        std::string line = std::string("help ") + consoleInput;
        RunConsoleLine(line);
        auto r = rpc.call("help", std::string("[\"") + consoleInput + "\"]");
        helpDetail = r.ok ? r.resultJson : r.error;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear out"))
        consoleOut.clear();

    ImGui::TextDisabled("History: %zu entries (re-type or re-select from list)", consoleHistory.size());
    ImGui::BeginChild("##out", ImVec2(0, 0), true);
    ImGui::TextUnformatted(consoleOut.empty() ? "(output)" : consoleOut.c_str());
    ImGui::EndChild();
    ImGui::EndChild();
    EndContentPage();
}

void App::DrawOptions()
{
    BeginContentPage("Options");
    ImGui::TextWrapped(
        "Connection applies immediately. Node knobs are saved and exported to a "
        "dogecoin.conf fragment (restart dogecoind to apply). Hybrid open-first is "
        "shared with the Start Menu launcher and the operator TUI.");

    if (ImGui::BeginTabBar("##optabs")) {
        if (ImGui::BeginTabItem("Main")) {
            optionsTab = 0;
            ImGui::TextUnformatted("Network");
            int net = (cfg.network == "test") ? 1 : (cfg.network == "regtest" ? 2 : 0);
            if (ImGui::RadioButton("Mainnet  (RPC 22555)", net == 0)) {
                if (cfg.network != "main") {
                    cfg.network = "main";
                    ApplyNetworkMode();
                    CollectUiToSettings();
                    SaveSettings(settingsPath, cfg);
                    optionsStatus = "Mainnet selected. Restart the node (File → Exit) if one is already running.";
                }
            }
            if (ImGui::RadioButton("Testnet  (RPC 44555, datadir testnet3)", net == 1)) {
                if (cfg.network != "test") {
                    cfg.network = "test";
                    ApplyNetworkMode();
                    CollectUiToSettings();
                    SaveSettings(settingsPath, cfg);
                    optionsStatus = "Testnet selected. Matrix theme is the default so you can see it. "
                                    "Stop mainnet first — one dogecoind. Then Start local dogecoind.";
                }
            }
            ImGui::TextDisabled(
                "Same node, different chain. Start Menu / Desktop: Dogecoin Core Pro Testnet. "
                "CLI: corepro-launch.exe --testnet   or   dogecoin-pro-gui.exe --ui gfx --testnet   or   "
                "dogecoin-cli -testnet   /   gpenode-tui --testnet");
            ImGui::Separator();
            if (cfg.network == "test")
                ImGui::TextDisabled("Fast Sync CDN is mainnet-only. Testnet IBD is from testnet peers.");
            ImGui::Checkbox("Prefer Fast Sync when available (prune + snapshot CDN)", &cfg.preferFastSync);
            ImGui::Checkbox("Prune block storage", &cfg.prune);
            ImGui::InputInt("Prune size (GB)", &cfg.pruneSizeGb);
            ImGui::InputInt("dbcache (MiB)", &cfg.dbCacheMb);
            ImGui::TextDisabled("Keep 1024–4096 MiB if this PC has the RAM. Fewer flushes during IBD.");
            ImGui::Text("Live engine (from node):  %s",
                        snap.dbEngine.empty() ? "(probe node)" : snap.dbEngine.c_str());
            if (ImGui::Checkbox("Next fresh start: MDBX (empty/wiped datadir only)", &cfg.preferMdbx)) {
                CollectUiToSettings();
                SaveSettings(settingsPath, cfg);
                if (datadirHint[0]) {
                    std::string conf = std::string(datadirHint) + "/dogecoin.conf";
                    UpsertNodeConfKey(conf, "dbengine", cfg.preferMdbx ? "mdbx" : "leveldb");
                }
            }
            ImGui::TextDisabled(
                "LevelDB remains the default for existing chainstate. "
                "MDBX from scratch: stop the node, backup wallet.dat, delete chainstate/ and "
                "blocks/, set this, restart, then Fast Sync. Or: -migratedb=mdbx -swapdb.");
            ImGui::InputText("Custom snapshot URL", snapshotUrl, sizeof(snapshotUrl));
            ImGui::InputText("Snapshot SHA-256", snapshotSha, sizeof(snapshotSha));
            ImGui::InputText("Datadir hint (for conf export)", datadirHint, sizeof(datadirHint));
            if (LooksLikeConsumerCloudPath(datadirHint)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.25f, 0.2f, 1.f));
                ImGui::TextWrapped("Live datadir cannot be on a cloud sync folder (OneDrive, Drive, iCloud, Dropbox, Proton, pCloud, MEGA, Nextcloud).");
                ImGui::PopStyleColor();
            } else if (LooksLikeOperatorFileStore(datadirHint)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.75f, 0.25f, 1.f));
                ImGui::TextWrapped("Server network disk (virtiofs / NFS). OK for operator archives — do not use as the live desktop datadir.");
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("Live datadir: local disk only.");
            }
            ImGui::Separator();
            ImGui::TextUnformatted("Cold offload (optional)");
            ImGui::SetNextItemWidth(-80);
            ImGui::InputText("##archive", archivePath, sizeof(archivePath));
            ImGui::SameLine();
#if defined(_WIN32)
            if (ImGui::Button("...##arch")) {
                char picked[512];
                std::snprintf(picked, sizeof(picked), "%s", archivePath);
                if (PickFolder(picked, sizeof(picked)))
                    std::snprintf(archivePath, sizeof(archivePath), "%s", picked);
            }
#else
            ImGui::TextDisabled("...");
#endif
            ImGui::TextDisabled("Finalized blk/rev copied here before prune deletes them. Live datadir stays local.");
            if (ImGui::CollapsingHeader("Server / operator only")) {
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("Snapshot dest (CDN origin / dumps)", snapshotDestPath, sizeof(snapshotDestPath));
                ImGui::TextDisabled("Vultr / NFS / extra disk on a headless node — e.g. /mnt/vfs/snapshots. Not a desktop setting.");
            }
            ImGui::Checkbox("Start on system login (Windows note - configure OS startup separately)",
                            &cfg.startAtLogin);
            ImGui::TextDisabled("Reverting prune requires re-downloading the chain.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Performance")) {
            optionsTab = 9;
            ImGui::TextUnformatted("Script verification threads  (-par)");
            ImGui::TextWrapped(
                "Already in the daemon. 0 = auto (all cores, max 16). "
                "Does not change consensus — only how many workers check signatures. "
                "Takes effect on the next clean node start (File → Exit, then reopen). "
                "Do not restart mid-IBD just for this.");
            int par = cfg.scriptThreads;
            if (par < 0) par = 0;
            if (par > 16) par = 16;
            if (ImGui::SliderInt("##par", &par, 0, 16, par == 0 ? "auto" : "%d")) {
                cfg.scriptThreads = par;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(par == 0 ? "auto" : "threads");
            if (ImGui::Button("Save -par to dogecoin.conf")) {
                CollectUiToSettings();
                SaveSettings(settingsPath, cfg);
                if (datadirHint[0]) {
                    std::string conf = std::string(datadirHint) + "/dogecoin.conf";
                    UpsertNodeConfKey(conf, "par", std::to_string(cfg.scriptThreads));
                    optionsStatus = "Wrote par=" + std::to_string(cfg.scriptThreads) +
                                    ". Next node start uses it. Leave IBD running.";
                } else {
                    optionsStatus = "Saved GUI setting. Set datadir hint to also write dogecoin.conf.";
                }
            }
            ImGui::Separator();
            ImGui::TextUnformatted("UTXO cache  (-dbcache)");
            ImGui::TextWrapped(
                "Same value as Options → Main. This is the safe prefetch: hot coins stay in RAM "
                "so IBD does not hit disk every transaction. 2048 MiB is already a strong default.");
            ImGui::InputInt("dbcache (MiB)##perf", &cfg.dbCacheMb);
            if (cfg.dbCacheMb < 300)
                cfg.dbCacheMb = 300;
            if (cfg.dbCacheMb > 16384)
                cfg.dbCacheMb = 16384;
            ImGui::TextDisabled("assumevalid stays on (default). Fast Sync is only for a fresh empty folder.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hybrid")) {
            optionsTab = 7;
            ImGui::TextWrapped(
                "Hybrid means this PC runs a full Dogecoin node (a server) plus the "
                "desktop wallet. You are not a light client. Window X / Hide to tray "
                "leave dogecoind running. File → Exit and tray Quit stop the node "
                "(RPC stop, then wait for flush). Never force-kill.");
            ImGui::TextWrapped(
                "Client is the same on Exit: stop the node safely. One dogecoind either way.");
            ImGui::Separator();
            ImGui::TextWrapped(
                "Which UI should Start Menu open first? Change here or in TUI Settings (H).");
            int hy = 0;
            if (cfg.hybridDefaultUi == "gfx") hy = 1;
            else if (cfg.hybridDefaultUi == "tui") hy = 2;
            if (ImGui::RadioButton("Ask every time", hy == 0))
                cfg.hybridDefaultUi = "ask";
            if (ImGui::RadioButton("Desktop GUI (ImGui)", hy == 1))
                cfg.hybridDefaultUi = "gfx";
            if (ImGui::RadioButton("Operator TUI", hy == 2))
                cfg.hybridDefaultUi = "tui";
            ImGui::TextDisabled("Stored in hybrid-ui.txt next to the binaries (not dogecoin.conf).");
            if (ImGui::Button("Save hybrid preference")) {
                CollectUiToSettings();
                std::string inst = settingsPath;
                auto slash = inst.find_last_of("/\\");
                if (slash != std::string::npos)
                    inst = inst.substr(0, slash);
                if (SaveHybridUiPref(inst, cfg.hybridDefaultUi))
                    optionsStatus = "Saved hybrid-ui.txt (" + cfg.hybridDefaultUi + ")";
                else
                    optionsStatus = "Could not write hybrid-ui.txt (try running as the installing user)";
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Wallet")) {
            optionsTab = 1;
            ImGui::Checkbox("Spend unconfirmed change", &cfg.spendZeroConfChange);
            ImGui::Checkbox("Enable coin control features (GUI preference)", &cfg.coinControl);
            ImGui::TextDisabled("Wallet keys and DB stay in dogecoind.");
            ImGui::Separator();
            if (snap.hasWallet && !snap.walletEncrypted) {
                ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f),
                                   "This wallet is not encrypted.");
                ImGui::TextWrapped(
                    "Anyone with this Windows user and RPC access can spend. "
                    "encryptwallet is optional but recommended after IBD.");
                ImGui::InputText("New passphrase", encryptPass1, sizeof(encryptPass1),
                                 ImGuiInputTextFlags_Password);
                ImGui::InputText("Repeat passphrase", encryptPass2, sizeof(encryptPass2),
                                 ImGuiInputTextFlags_Password);
                const bool canEnc = encryptPass1[0] && std::strcmp(encryptPass1, encryptPass2) == 0 &&
                                    snap.connected;
                if (!canEnc)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Encrypt wallet")) {
                    std::ostringstream p;
                    p << "[\"" << JsonEscape(encryptPass1) << "\"]";
                    auto r = rpc.call("encryptwallet", p.str());
                    optionsStatus = r.ok ? "Wallet encrypted. Node may restart the wallet — unlock to spend."
                                         : ("encryptwallet: " + r.error);
                    encryptPass1[0] = encryptPass2[0] = 0;
                }
                if (!canEnc)
                    ImGui::EndDisabled();
                if (encryptPass1[0] && std::strcmp(encryptPass1, encryptPass2) != 0)
                    ImGui::TextDisabled("Passphrases do not match.");
            } else if (snap.walletEncrypted && snap.walletLocked) {
                ImGui::TextWrapped("Wallet is encrypted and locked. Unlock from Console: walletpassphrase.");
            } else if (snap.walletEncrypted) {
                ImGui::TextDisabled("Wallet is encrypted.");
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Privacy")) {
            optionsTab = 8;
            const bool socks = TorSocksListening(9050);
            const std::string torExe = FindTorExecutable();
            const std::string dropDir = ExpectedTorDir();
            const std::string dropExe = ExpectedTorExe();
            ImGui::TextUnformatted("Optional — off unless you turn it on");
            ImGui::TextWrapped(
                "Core Pro does not ship Tor (keeps Defender / AV quiet). "
                "If you want it, drop the Tor Expert Bundle yourself next to the install:");
            ImGui::Text("    %s", dropExe.empty() ? "Dogecoin\\tor\\tor.exe" : dropExe.c_str());
            ImGui::TextWrapped(
                "Unzip so tor.exe sits in that tor\\ folder (same place as dogecoin-pro-gui.exe, "
                "one directory down). Then turn on the checkbox below. Core Pro starts Tor "
                "only when that option is on (including the next launch). Meme Stream and "
                "Arcade stay on normal HTTPS.");
            if (ImGui::Button("Open the tor folder")) {
#if defined(_WIN32)
                InvalidateTorStatusCache();
                if (!dropDir.empty()) {
                    CreateDirectoryA(dropDir.c_str(), nullptr);
                    const std::string readme = dropDir + "\\README.txt";
                    if (GetFileAttributesA(readme.c_str()) == INVALID_FILE_ATTRIBUTES) {
                        std::ofstream rf(readme.c_str());
                        if (rf) {
                            rf << "Put tor.exe here (Tor Expert Bundle).\n"
                               << "Core Pro does not ship Tor. This is optional.\n"
                               << "Then: Options → Privacy → enable the P2P checkbox.\n";
                        }
                    }
                    ShellExecuteA(nullptr, "open", dropDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
#endif
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy path"))
                CopyToClipboard(dropExe.empty() ? dropDir : dropExe);
            ImGui::Separator();
            if (torExe.empty())
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.f),
                                   "tor.exe not found yet — folder is empty until you copy it in.");
            else
                ImGui::Text("Found:  %s", torExe.c_str());
            ImGui::TextColored(socks ? ImVec4(0.4f, 0.9f, 0.4f, 1.f) : ImVec4(0.9f, 0.7f, 0.2f, 1.f),
                               socks ? "SOCKS 127.0.0.1:9050 is up"
                                     : "SOCKS 127.0.0.1:9050 is down");
            ImGui::Separator();
            bool want = cfg.p2pViaTor;
            if (ImGui::Checkbox("Route blockchain P2P through local Tor (optional)", &want))
                ApplyP2pTorPref(want);
            ImGui::TextDisabled(
                "Unchecked = normal clearnet. Never forced. Tor is started only when this is on.");
            if (snap.connected) {
                ImGui::Text("Live node proxy:  %s",
                            snap.p2pProxy.empty() ? "(none — still clearnet P2P)"
                                                  : snap.p2pProxy.c_str());
                if (cfg.p2pViaTor && snap.p2pProxy.find("9050") == std::string::npos) {
                    if (snap.initialBlockDownload)
                        ImGui::TextWrapped(
                            "Preference saved. P2P stays clearnet until a clean node restart "
                            "after IBD (File → Exit, then reopen). Do not force-kill.");
                    else
                        ImGui::TextWrapped(
                            "Preference saved. Restart the node (File → Exit, then reopen) "
                            "to put P2P on Tor. Do not restart mid-flush.");
                }
                if (!snap.onionAddress.empty())
                    ImGui::TextDisabled("Local onion: %s", snap.onionAddress.c_str());
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Network")) {
            optionsTab = 2;
            ImGui::Checkbox("Allow incoming connections (listen)", &cfg.listen);
            ImGui::Checkbox("Map port using UPnP", &cfg.upnp);
            ImGui::Checkbox("DNS seed", &cfg.dnsSeed);
            ImGui::InputInt("Max connections", &cfg.maxConnections);
            if (cfg.p2pViaTor) {
                ImGui::BeginDisabled();
                bool on = true;
                ImGui::Checkbox("Connect through SOCKS5 proxy", &on);
                ImGui::Text("Proxy:  127.0.0.1:9050  (set by Options → Privacy)");
                ImGui::EndDisabled();
            } else {
                ImGui::Checkbox("Connect through SOCKS5 proxy", &cfg.proxyEnabled);
                ImGui::InputText("Proxy IP", proxyIp, sizeof(proxyIp));
                ImGui::InputInt("Proxy port", &cfg.proxyPort);
                ImGui::Checkbox("Separate Tor SOCKS5", &cfg.proxyTorEnabled);
                ImGui::InputText("Tor proxy IP", proxyTorIp, sizeof(proxyTorIp));
                ImGui::InputInt("Tor proxy port", &cfg.proxyTorPort);
            }
            ImGui::TextDisabled(
                "Optional. Needs a local Tor (Expert Bundle in Dogecoin\\tor\\tor.exe) "
                "listening on 9050 or Tor Browser on 9150. If the proxy is down, P2P "
                "does not fall back to clearnet — it just fails to connect. "
                "Everyday switch is Options → Privacy (off by default).");
            if (snap.connected && ImGui::Button("RPC: setnetworkactive true")) {
                auto r = rpc.call("setnetworkactive", "[true]");
                optionsStatus = r.ok ? "network active" : r.error;
            }
            ImGui::SameLine();
            if (snap.connected && ImGui::Button("RPC: setnetworkactive false")) {
                auto r = rpc.call("setnetworkactive", "[false]");
                optionsStatus = r.ok ? "network paused" : r.error;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Window")) {
            optionsTab = 3;
            ImGui::Checkbox("Minimize (_) also sends the window to the tray", &cfg.minimizeToTray);
            ImGui::Checkbox("Show tray balloon / notify-send", &cfg.showTrayNotifications);
            if (ImGui::Checkbox("Don't remind me that X sends Core Pro to the tray",
                                &cfg.trayHintDontShowAgain)) {
                CollectUiToSettings();
                SaveSettings(settingsPath, cfg);
            }
            ImGui::TextDisabled(
                "X always goes to the tray (node stays up). There is one tray icon — this "
                "window's. File → Exit and that icon's Quit and stop node stop the "
                "Windows service if present, then RPC-stop dogecoind and wait for flush.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Display")) {
            optionsTab = 4;
            ImGui::InputText("Unit label", displayUnit, sizeof(displayUnit));
            ImGui::TextDisabled("Language packs: use system locale for now; .ts later.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Theme")) {
            optionsTab = 5;
            int t = (int)theme;
            if (ImGui::Combo("Theme", &t, "Gold Dark\0Matrix\0Dim\0")) {
                theme = (ProTheme)t;
                ApplyProTheme(theme);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Connection")) {
            optionsTab = 6;
            ImGui::InputText("RPC Host", rpcHost, sizeof(rpcHost));
            ImGui::InputInt("RPC Port", &rpcPort);
            ImGui::InputText("Cookie path", cookiePath, sizeof(cookiePath));
            ImGui::TextDisabled(
                "Auth prefers .cookie, then rpcuser/rpcpassword from dogecoin.conf. "
                "The GUI ini does not store the RPC password.");
            ImGui::InputText("RPC User", rpcUser, sizeof(rpcUser));
            ImGui::InputText("RPC Password (not saved in GUI settings)", rpcPass, sizeof(rpcPass),
                             ImGuiInputTextFlags_Password);
            ImGui::Checkbox("Stop dogecoind on other Exit paths (recommended)", &stopNodeOnExit);
            ImGui::TextDisabled(
                "File → Exit and tray Quit always stop the node. This checkbox is for "
                "first-run Cancel and similar paths. Hybrid/Server start with it off.");
            if (ImGui::Button("Reconnect splash")) {
                offlineMode = false;
                showBootSplash = true;
                InitBootChecklist();
                autoStartTried = false;
                bootStartTime = 0;
                helpLoaded = false;
                lastProbeTime = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Probe now")) {
                lastProbeTime = 0;
                TickNodeProbe();
            }
            ImGui::SameLine();
            if (ImGui::Button("Start local dogecoind")) {
                TryStartLocalNode();
            }
            ImGui::TextWrapped("%s", nodeLaunchStatus.c_str());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Save settings")) {
        CollectUiToSettings();
        if (SaveSettings(settingsPath, cfg))
            optionsStatus = "Saved " + settingsPath;
        else
            optionsStatus = "Save failed";
        SyncRpcConfig();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export node conf fragment")) {
        CollectUiToSettings();
        if (ExportNodeConfFragment(nodeConfExportPath, cfg))
            optionsStatus = "Wrote " + nodeConfExportPath + " - merge into dogecoin.conf and restart node";
        else
            optionsStatus = "Export failed";
    }
    ImGui::SameLine();
    ImGui::Checkbox("ImGui demo", &showDemo);
    if (!optionsStatus.empty())
        ImGui::TextWrapped("%s", optionsStatus.c_str());
    EndContentPage();
}

void App::DrawArcade()
{
    // Full content area is covered by WebView2 (the real GPE cabinet).
    // This ImGui page only holds a thin chrome strip when embed is loading/failed.
    BeginContentPage("Doge Arcade");

    const bool embedded = arcadeWeb.ready() && arcadeWeb.visible();
    if (embedded) {
        // Leave almost all space empty so the native WebView shows through;
        // a 1px dummy keeps the window layout valid.
        ImGui::Dummy(ImVec2(1, 1));
        ImGui::TextDisabled("%s", arcadeWeb.status().c_str());
    } else {
        if (icons.count("insert_coin") && icons["insert_coin"].ok()) {
            ImGui::Image((ImTextureID)(intptr_t)icons["insert_coin"].id, ImVec2(48, 48));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::TextUnformatted("DOGECOIN CORE PRO ARCADE");
        ImGui::TextDisabled("Loading in-panel cabinet…  %s", ArcadeHost::kUaToken);
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", arcadeEmbedStatus.empty() ? arcadeWeb.status().c_str()
                                                           : arcadeEmbedStatus.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Retry embed", ImVec2(140, 0))) {
#if defined(_WIN32)
            arcadeWeb.Shutdown();
            arcadeNavStarted = false;
            UpdateArcadeEmbed();
#endif
        }
        ImGui::SameLine();
        if (ImGui::Button("Open external (UA unlock)", ImVec2(200, 0))) {
            const std::string url = arcadeHubUrl.empty() ? ArcadeHost::kHubUrl : arcadeHubUrl;
            ArcadeHost::LaunchCabinetUnlocked(url);
        }
        ImGui::TextDisabled("Need Edge WebView2 Runtime + WebView2Loader.dll next to the exe.");
    }
    EndContentPage();
}

static std::string JsonUnquote(std::string a)
{
    if (a.size() >= 2 && a.front() == '"')
        a = a.substr(1, a.size() - 2);
    return a;
}

std::string App::InvoicesPath() const
{
    std::string p = settingsPath;
    auto slash = p.find_last_of("/\\");
    if (slash != std::string::npos)
        p = p.substr(0, slash + 1);
    else
        p.clear();
    return p + "business-invoices.txt";
}

void App::LoadBusinessInvoices()
{
    invoices.clear();
    std::ifstream f(InvoicesPath());
    if (!f)
        return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string> c;
        size_t start = 0;
        for (size_t i = 0; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == '\t') {
                c.push_back(line.substr(start, i - start));
                start = i + 1;
            }
        }
        if (c.size() < 6)
            continue;
        BizInvoice inv;
        inv.id = c[0];
        inv.status = c[1];
        inv.amount = std::atof(c[2].c_str());
        inv.created = std::atoll(c[3].c_str());
        inv.address = c[4];
        inv.label = c[5];
        if (c.size() > 6)
            inv.note = c[6];
        invoices.push_back(inv);
    }
}

void App::SaveBusinessInvoices() const
{
    std::ofstream f(InvoicesPath(), std::ios::trunc);
    if (!f)
        return;
    f << "# id\tstatus\tamount\tcreated\taddress\tlabel\tnote\n";
    for (const auto& inv : invoices) {
        std::string lab = inv.label, note = inv.note;
        for (char& ch : lab)
            if (ch == '\t' || ch == '\n')
                ch = ' ';
        for (char& ch : note)
            if (ch == '\t' || ch == '\n')
                ch = ' ';
        f << inv.id << '\t' << inv.status << '\t' << inv.amount << '\t'
          << inv.created << '\t' << inv.address << '\t' << lab << '\t' << note << '\n';
    }
}

static std::string UrlEncodeLabel(const std::string& s)
{
    std::ostringstream o;
    o << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            o << (char)c;
        else
            o << '%' << std::setw(2) << std::setfill('0') << (int)c;
    }
    return o.str();
}

std::string App::InvoiceUri(const std::string& address, double amount, const std::string& label) const
{
    std::ostringstream u;
    u << "dogecoin:" << address;
    bool q = false;
    if (amount > 0) {
        u << "?amount=" << amount;
        q = true;
    }
    if (!label.empty()) {
        u << (q ? "&" : "?") << "label=" << UrlEncodeLabel(label);
    }
    return u.str();
}

void App::DrawQrCode(const std::string& payload, float sizePx)
{
    const auto qr = QrEncode(payload);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (qr.empty()) {
        dl->AddRectFilled(p, ImVec2(p.x + sizePx, p.y + sizePx), IM_COL32(255, 255, 255, 255), 4.0f);
        dl->AddRect(p, ImVec2(p.x + sizePx, p.y + sizePx), IM_COL32(180, 140, 40, 200), 4.0f);
        ImGui::Dummy(ImVec2(sizePx, sizePx));
        ImGui::TextDisabled("QR unavailable (payload too long). Copy the address instead.");
        return;
    }
    const int n = (int)qr.size();
    const int quiet = 2;
    const int cells = n + quiet * 2;
    const float cell = sizePx / (float)cells;
    dl->AddRectFilled(p, ImVec2(p.x + sizePx, p.y + sizePx), IM_COL32(255, 255, 255, 255), 4.0f);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            if (!(qr[y][x] & 1))
                continue;
            const float x0 = p.x + (x + quiet) * cell;
            const float y0 = p.y + (y + quiet) * cell;
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + cell + 0.6f, y0 + cell + 0.6f), IM_COL32(12, 10, 8, 255));
        }
    }
    ImGui::Dummy(ImVec2(sizePx, sizePx));
}

bool App::CreateBusinessInvoice(const std::string& label, double amount, const std::string& note)
{
    if (!snap.hasWallet) {
        businessStatus = "Wallet RPC not available.";
        return false;
    }
    std::string lab = label.empty() ? "Invoice" : label;
    std::string params = "[\"" + lab + "\"]";
    auto r = rpc.call("getnewaddress", params);
    if (!r.ok) {
        businessStatus = r.error.empty() ? "getnewaddress failed" : r.error;
        return false;
    }
    BizInvoice inv;
    inv.id = std::to_string((long long)time(nullptr)) + "-" + std::to_string(invoices.size() + 1);
    inv.label = lab;
    inv.address = JsonUnquote(r.resultJson);
    inv.note = note;
    inv.status = "open";
    inv.amount = amount < 0 ? 0 : amount;
    inv.created = (long long)time(nullptr);
    invoices.insert(invoices.begin(), inv);
    invoiceSel = 0;
    SaveBusinessInvoices();
    businessStatus = "Invoice created: " + inv.address;
    return true;
}

void App::TickBusinessWatch()
{
    if (invoices.empty() || !snap.hasWallet || !snap.connected)
        return;
    const double now = ImGui::GetTime();
    if (now - lastInvoiceWatch < 4.0)
        return;
    lastInvoiceWatch = now;
    bool changed = false;
    for (auto& inv : invoices) {
        if (inv.status != "open" || inv.address.empty())
            continue;
        std::ostringstream p;
        p << "[\"" << inv.address << "\",0]";
        auto r = rpc.call("getreceivedbyaddress", p.str());
        if (!r.ok)
            continue;
        double got = std::atof(JsonUnquote(r.resultJson).c_str());
        if (got <= 0)
            continue;
        if (inv.amount <= 0 || got + 1e-8 >= inv.amount) {
            inv.status = "paid";
            changed = true;
            businessStatus = "Payment detected — marked paid: " + inv.label;
        }
    }
    if (changed)
        SaveBusinessInvoices();
}

void App::EnsureMemeAuthor()
{
    if (memeAuthor[0])
        return;
    if (!cfg.memeAuthor.empty()) {
        std::snprintf(memeAuthor, sizeof(memeAuthor), "%s", cfg.memeAuthor.c_str());
        return;
    }
    if (!snap.hasWallet)
        return;
    auto r = rpc.call("getnewaddress", "[\"Meme Stream author\"]");
    if (!r.ok)
        return;
    std::string a = JsonUnquote(r.resultJson);
    if (a.empty())
        return;
    std::snprintf(memeAuthor, sizeof(memeAuthor), "%s", a.c_str());
    cfg.memeAuthor = a;
    CollectUiToSettings();
    SaveSettings(settingsPath, cfg);
}

void App::TipMemeCreator(const std::string& address)
{
    if (address.empty())
        return;
    std::snprintf(memeTipAddr, sizeof(memeTipAddr), "%s", address.c_str());
    if (!memeTipAmt[0])
        std::snprintf(memeTipAmt, sizeof(memeTipAmt), "1");
    memeTab = 1;
    activeNav = "meme";
}

void App::StartMemeFeed()
{
    const unsigned gen = ++memeJobGen;
    memeBusy = true;
    lastMemeFetch = ImGui::GetTime();
    memeFeedStatus = "Loading feed...";
    if (memeThread.joinable())
        memeThread.detach();
    memeThread = std::thread([this, gen]() {
        const std::string url = std::string(kMemeApiBase) + kMemeFeedPath + "24";
        MemeHttpReply r = MemeHttpGet(url);
        if (gen != memeJobGen.load())
            return;
        std::vector<MemeItem> items;
        MemeItem ofDay;
        std::string err;
        if (!r.ok) {
            err = r.error.empty() ? "Feed request failed" : r.error;
            if (r.status)
                err += " (HTTP " + std::to_string(r.status) + ")";
        } else if (!MemeParseFeed(r.body, items, &ofDay, err)) {
            if (err.empty())
                err = "Could not parse feed";
        }
        {
            std::lock_guard<std::mutex> lock(memeMu);
            if (gen != memeJobGen.load())
                return;
            memeItemsPending = items;
            memeOfTheDayPending = ofDay;
            memePendingError = err;
            memePendingStatus = err.empty()
                                    ? ("Live  ·  " + std::to_string(items.size()) + " posts")
                                    : err;
            memeFeedDirty = true;
        }
        if (!err.empty()) {
            if (gen == memeJobGen.load())
                memeBusy = false;
            return;
        }
        std::map<std::string, std::string> images;
        int n = 0;
        auto grab = [&](const MemeItem& it) {
            if (gen != memeJobGen.load())
                return;
            if (it.id.empty() || it.imageUrl.empty() || n >= 8)
                return;
            if (images.count(it.id))
                return;
            MemeHttpReply img = MemeHttpGet(it.imageUrl);
            if (!img.ok || img.body.size() < 24)
                return;
            if (img.body[0] == '<' || img.body.compare(0, 5, "<html") == 0 ||
                img.body.compare(0, 9, "<!DOCTYPE") == 0)
                return;
            images[it.id] = std::move(img.body);
            n++;
        };
        if (!ofDay.id.empty())
            grab(ofDay);
        for (const auto& it : items)
            grab(it);
        std::lock_guard<std::mutex> lock(memeMu);
        if (gen != memeJobGen.load())
            return;
        memePendingImages = std::move(images);
        memeFeedDirty = true;
        memeBusy = false;
    });
}

void App::ClearMemeImage()
{
    memeImageName.clear();
    memeImageBytes.clear();
    memeImageMime.clear();
    DestroyTexture(memeImagePreview);
}

void App::PickMemeImage()
{
    char path[512] = {};
#if defined(_WIN32)
    OPENFILENAMEA ofn {};
    ofn.lStructSize = sizeof(ofn);
    if (hostWindow)
        ofn.hwndOwner = glfwGetWin32Window(hostWindow);
    ofn.lpstrFilter = "Images (PNG, JPEG, GIF, WebP)\0*.png;*.jpg;*.jpeg;*.gif;*.webp\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = sizeof(path);
    ofn.lpstrTitle = "Choose image (max 69 KiB)";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn))
        return;
#else
    FILE* z = popen("zenity --file-selection --title='Choose image (max 69 KiB)' "
                    "--file-filter='Images | *.png *.jpg *.jpeg *.gif *.webp' 2>/dev/null",
                    "r");
    if (!z) {
        memeStatus = "No file picker (install zenity) — type a path is not wired.";
        return;
    }
    if (!std::fgets(path, sizeof(path), z)) {
        pclose(z);
        return;
    }
    pclose(z);
    size_t n = std::strlen(path);
    while (n && (path[n - 1] == '\n' || path[n - 1] == '\r'))
        path[--n] = 0;
    if (!path[0])
        return;
#endif
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        memeStatus = "Could not open image.";
        return;
    }
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (bytes.size() > (size_t)kMemeMaxImageBytes) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "Image too large (%u bytes). Max is 69 KiB (%d bytes).",
                      (unsigned)bytes.size(), kMemeMaxImageBytes);
        memeStatus = msg;
        return;
    }
    if (bytes.size() < 8) {
        memeStatus = "File is too small to be an image.";
        return;
    }
    ClearMemeImage();
    memeImageBytes = std::move(bytes);
    const char* slash = std::strrchr(path, '\\');
    if (!slash)
        slash = std::strrchr(path, '/');
    memeImageName = slash ? (slash + 1) : path;
    memeImageMime = MemeGuessImageMime(memeImageName, memeImageBytes);
    memeImagePreview = LoadTextureFromMemory(memeImageBytes.data(), (int)memeImageBytes.size());
    char ok[192];
    std::snprintf(ok, sizeof(ok), "Attached %s (%u bytes%s)",
                  memeImageName.c_str(), (unsigned)memeImageBytes.size(),
                  memeImagePreview.ok() ? "" : ", preview unavailable");
    memeStatus = ok;
}

void App::StartMemePublish()
{
    EnsureMemeAuthor();
    if (!memeAuthor[0]) {
        memeStatus = "Need a wallet receive address to publish (author / tip target).";
        return;
    }
    if (!memePubTitle[0] && !memePubBody[0] && memeImageBytes.empty()) {
        memeStatus = "Write a title or body, or attach an image.";
        return;
    }
    if (memeImageBytes.size() > (size_t)kMemeMaxImageBytes) {
        memeStatus = "Image too large. Max is 69 KiB.";
        return;
    }
    const unsigned gen = ++memeJobGen;
    memeBusy = true;
    std::string title = memePubTitle;
    std::string body = memePubBody;
    std::string wallet = memeAuthor;
    std::string img = memeImageBytes;
    std::string imgName = memeImageName;
    std::string imgMime = memeImageMime;
    memeStatus = img.empty() ? "Publishing..." : "Publishing with image...";
    if (memeThread.joinable())
        memeThread.detach();
    memeThread = std::thread([this, title, body, wallet, img, imgName, imgMime, gen]() {
        // Reconstruct the obfuscated Core key only here. Never store it on App.
        std::string key = MemePublishKey();
        const std::string url = std::string(kMemeApiBase) + kMemePublishPath;
        std::vector<std::pair<std::string, std::string>> hdrs = {{"X-MemeStream-Key", key}};
        MemeHttpReply r;
        if (img.empty()) {
            r = MemeHttpPostJson(url, MemeBuildPublishJson(title, body, wallet), hdrs);
        } else {
            std::string ctype;
            const std::string multi = MemeBuildPublishMultipart(
                title, body, wallet, imgName, imgMime, img, ctype);
            r = MemeHttpPost(url, ctype, multi, hdrs);
        }
#if defined(_WIN32)
        if (!key.empty())
            SecureZeroMemory(&key[0], key.size());
#endif
        key.assign(key.size(), '\0');
        key.clear();
        if (gen != memeJobGen.load())
            return;
        std::string err;
        if (!r.ok) {
            err = r.error;
            if (r.status == 401)
                err = "Unauthorized (client_only). Built-in publish key was rejected.";
            else if (err.empty())
                err = "Publish failed (HTTP " + std::to_string(r.status) + ")";
        }
        std::lock_guard<std::mutex> lock(memeMu);
        if (gen != memeJobGen.load())
            return;
        memePendingError = err;
        memePendingStatus = err.empty() ? "Published. Refreshing feed..." : err;
        memeFeedDirty = true;
        memeBusy = false;
        if (err.empty())
            memeItemsPending.clear();
    });
}

void App::StartMemeLike(const std::string& id)
{
    if (id.empty())
        return;
    const unsigned gen = ++memeJobGen;
    memeBusy = true;
    if (memeThread.joinable())
        memeThread.detach();
    EnsureMemeAuthor();
    std::string wallet = memeAuthor;
    memeThread = std::thread([this, id, wallet, gen]() {
        if (gen != memeJobGen.load())
            return;
        const std::string url = std::string(kMemeApiBase) +
                                "/api/public/memestream/items/" + id + "/like";
        std::string json = wallet.empty() ? "{}" : ("{\"wallet\":\"" + MemeJsonEscape(wallet) + "\"}");
        std::vector<std::pair<std::string, std::string>> hdrs;
        if (!wallet.empty())
            hdrs.push_back({"X-Doge-Address", wallet});
        MemeHttpReply r = MemeHttpPostJson(url, json, hdrs);
        if (gen != memeJobGen.load())
            return;
        std::lock_guard<std::mutex> lock(memeMu);
        if (gen != memeJobGen.load())
            return;
        if (!r.ok)
            memePendingError = r.error.empty() ? "Like failed" : r.error;
        else {
            memePendingStatus = "Wow sent.";
            memePendingLikeId = id;
        }
        memeFeedDirty = true;
        memeBusy = false;
    });
}

void App::TickMemeStream()
{
    const double now = ImGui::GetTime();
    if (memeBusy.load() && memeItems.empty() && lastMemeFetch > 0 &&
        now - lastMemeFetch > 18.0) {
        ++memeJobGen;
        memeBusy = false;
        memeFeedStatus = "Feed timed out. Click Refresh.";
        lastMemeFetch = now;
    }

    bool refreshAfterPublish = false;
    std::string likedId;
    {
        std::lock_guard<std::mutex> lock(memeMu);
        if (memeFeedDirty) {
            memeFeedDirty = false;
            if (!memePendingError.empty())
                memeStatus = memePendingError;
            else if (!memePendingStatus.empty())
                memeStatus = memePendingStatus;
            if (!memePendingStatus.empty())
                memeFeedStatus = memePendingStatus;
            if (memePendingError.empty() &&
                memePendingStatus.find("Published") != std::string::npos)
                refreshAfterPublish = true;
            if (!memeItemsPending.empty()) {
                memeItems = std::move(memeItemsPending);
                memeOfTheDay = memeOfTheDayPending;
            }
            if (!memePendingLikeId.empty()) {
                likedId = memePendingLikeId;
                memePendingLikeId.clear();
            }
            for (auto& kv : memePendingImages) {
                auto it = memeThumbs.find(kv.first);
                if (it != memeThumbs.end())
                    DestroyTexture(it->second);
                if (kv.second.size() > 24 && kv.second[0] != '<')
                    memeThumbs[kv.first] = LoadTextureFromMemory(kv.second.data(), (int)kv.second.size());
            }
            memePendingImages.clear();
        }
    }
    if (!likedId.empty()) {
        for (auto& it : memeItems)
            if (it.id == likedId)
                it.likes += 1;
        if (memeOfTheDay.id == likedId)
            memeOfTheDay.likes += 1;
    }
    if (refreshAfterPublish) {
        memePubTitle[0] = 0;
        memePubBody[0] = 0;
        ClearMemeImage();
    }
}

void App::DrawMemeCard(const MemeItem& item, bool compact)
{
    ImGui::PushID(item.id.empty() ? item.title.c_str() : item.id.c_str());
    ImGui::BeginChild("##memecard", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    if (item.featured)
        ImGui::TextColored(ImVec4(0.92f, 0.72f, 0.18f, 1.f), "Featured");
    ImGui::TextUnformatted(item.title.empty() ? "(untitled)" : item.title.c_str());
    auto thumb = memeThumbs.find(item.id);
    if (thumb != memeThumbs.end() && thumb->second.ok()) {
        const float maxW = compact ? 260.0f : 360.0f;
        const float maxH = compact ? 120.0f : 180.0f;
        float w = (float)thumb->second.width;
        float h = (float)thumb->second.height;
        if (w > maxW) {
            h *= maxW / w;
            w = maxW;
        }
        if (h > maxH) {
            w *= maxH / h;
            h = maxH;
        }
        ImGui::Image((ImTextureID)(intptr_t)thumb->second.id, ImVec2(w, h));
    }
    if (!item.body.empty()) {
        std::string body = item.body;
        if (compact && body.size() > 90)
            body = body.substr(0, 87) + "...";
        ImGui::TextWrapped("%s", body.c_str());
    }
    const std::string tip = item.tipAddress.empty() ? item.author : item.tipAddress;
    if (!tip.empty()) {
        std::string shortTip = tip;
        if (shortTip.size() > 16)
            shortTip = shortTip.substr(0, 6) + "..." + shortTip.substr(shortTip.size() - 4);
        ImGui::TextDisabled("Tip → %s   ·   Wow %d", shortTip.c_str(), item.likes);
    } else {
        ImGui::TextDisabled("Wow %d", item.likes);
    }
    if (ImGui::SmallButton("Wow"))
        StartMemeLike(item.id);
    ImGui::SameLine();
    if (ImGui::SmallButton("Tip") && !tip.empty())
        TipMemeCreator(tip);
    if (!compact) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy address") && !tip.empty())
            CopyToClipboard(tip);
    }
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 4));
}

void App::DrawMemeRail()
{
    ImGui::TextUnformatted("Meme Stream");
    ImGui::SameLine();
    if (ImGui::SmallButton("Open full page")) {
        activeNav = "meme";
        memeTab = 0;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload")) {
#if defined(_WIN32)
        if (arcadeWeb.ready())
            arcadeWeb.Navigate(memeStreamUrl);
#endif
    }
    ImGui::TextDisabled("Live GPE stream  ·  Submit / tip is on the full page");
    ImGui::Dummy(ImVec2(1, ImMax(8.0f, ImGui::GetContentRegionAvail().y - 4.0f)));
}

void App::DrawMemeStream()
{
    BeginContentPage("Meme Stream");
    if (ImGui::BeginTabBar("##memetabs")) {
        if (ImGui::BeginTabItem("Stream")) {
            memeTab = 0;
            ImGui::TextDisabled("Full GPE site  ·  Submit a meme on the next tab (max 69 KiB)");
            ImGui::SameLine();
            if (ImGui::SmallButton("Open in browser"))
                OpenUrl(memeStreamUrl);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reload site")) {
#if defined(_WIN32)
                if (arcadeWeb.ready())
                    arcadeWeb.Navigate(memeStreamUrl);
#endif
            }
#if defined(_WIN32)
            if (!(arcadeWeb.ready() && arcadeWeb.visible())) {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", arcadeEmbedStatus.empty() ? arcadeWeb.status().c_str()
                                                                   : arcadeEmbedStatus.c_str());
                if (ImGui::Button("Retry embed")) {
                    arcadeWeb.Shutdown();
                    arcadeNavStarted = false;
                    embedNavUrl.clear();
                    UpdateArcadeEmbed();
                }
            } else {
                ImGui::Dummy(ImVec2(1, 1));
            }
#else
            ImGui::TextWrapped("On this OS the site opens in your browser.");
#endif
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Submit")) {
            memeTab = 1;
            EnsureMemeAuthor();
            ImGui::TextWrapped(
                "Posts from Core use the built-in client key. The web site is view-only. "
                "Your receive address is the author so others can tip you on-chain.");
            ImGui::Text("Author / tip target:  %s",
                        memeAuthor[0] ? memeAuthor : "(unlock wallet to create an address)");
            if (ImGui::SmallButton("New author address") && snap.hasWallet) {
                memeAuthor[0] = 0;
                cfg.memeAuthor.clear();
                EnsureMemeAuthor();
            }
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("Title", memePubTitle, sizeof(memePubTitle));
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextMultiline("Body", memePubBody, sizeof(memePubBody), ImVec2(-1, 90));
            if (ImGui::Button("Choose image..."))
                PickMemeImage();
            ImGui::SameLine();
            if (ImGui::Button("Clear image") && !memeImageBytes.empty())
                ClearMemeImage();
            if (memeImageBytes.empty())
                ImGui::TextDisabled("No image (optional, max 69 KiB JPEG / PNG / GIF / WebP)");
            else {
                ImGui::Text("Attached: %s  (%u / %d bytes)",
                            memeImageName.c_str(),
                            (unsigned)memeImageBytes.size(), kMemeMaxImageBytes);
                if (memeImagePreview.ok()) {
                    float w = (float)memeImagePreview.width;
                    float h = (float)memeImagePreview.height;
                    const float maxW = 280.0f, maxH = 160.0f;
                    if (w > maxW) {
                        h *= maxW / w;
                        w = maxW;
                    }
                    if (h > maxH) {
                        w *= maxH / h;
                        h = maxH;
                    }
                    ImGui::Image((ImTextureID)(intptr_t)memeImagePreview.id, ImVec2(w, h));
                }
            }
            const bool canPub = snap.hasWallet && memeAuthor[0] &&
                                (memePubTitle[0] || memePubBody[0] || !memeImageBytes.empty());
            if (!canPub)
                ImGui::BeginDisabled();
            if (ImGui::Button("Submit meme", ImVec2(180, 0)))
                StartMemePublish();
            if (!canPub)
                ImGui::EndDisabled();
            if (!memeStatus.empty())
                ImGui::TextWrapped("%s", memeStatus.c_str());
            ImGui::TextDisabled(
                "Publish uses the built-in Core client key (never shown). "
                "Image field is multipart \"image\", max 69 KiB.");

            ImGui::Separator();
            ImGui::TextUnformatted("Tip a creator");
            ImGui::TextDisabled("On-chain sendtoaddress. Keys stay in dogecoind.");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("Creator address", memeTipAddr, sizeof(memeTipAddr));
            ImGui::SetNextItemWidth(180);
            ImGui::InputText("Amount", memeTipAmt, sizeof(memeTipAmt));
            ImGui::SameLine();
            ImGui::TextUnformatted(displayUnit);
            const bool canTip = snap.hasWallet && memeTipAddr[0] && memeTipAmt[0];
            if (!canTip)
                ImGui::BeginDisabled();
            if (ImGui::Button("Send tip", ImVec2(140, 0))) {
                std::ostringstream p;
                p << "[\"" << memeTipAddr << "\"," << memeTipAmt << ",\"memestream tip\"]";
                auto r = rpc.call("sendtoaddress", p.str());
                memeStatus = r.ok ? ("Tipped — txid " + r.resultJson) : ("Failed: " + r.error);
                statusLine = memeStatus;
            }
            if (!canTip)
                ImGui::EndDisabled();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    EndContentPage();
}

void App::DrawBusiness()
{
    BeginContentPage("Doge Business");
    ImGui::TextDisabled("Local merchant tools. Keys stay in this wallet — not an external POS server.");
    if (ImGui::BeginTabBar("##biztabs")) {
        int openN = 0, paidN = 0;
        double vol = 0;
        for (const auto& inv : invoices) {
            if (inv.status == "open")
                openN++;
            if (inv.status == "paid") {
                paidN++;
                vol += inv.amount;
            }
        }
        if (ImGui::BeginTabItem("Dashboard")) {
            businessTab = 0;
            ImGui::Text("Balance:  %.4f %s", snap.hasWallet ? snap.balance : 0.0, displayUnit);
            ImGui::Text("Open invoices:  %d", openN);
            ImGui::Text("Paid invoices:  %d", paidN);
            ImGui::Text("Recorded paid volume:  %.4f %s", vol, displayUnit);
            ImGui::Spacing();
            ImGui::TextWrapped("%s", businessStatus.empty()
                                         ? "Auto-watch marks open invoices paid when this wallet receives to their address."
                                         : businessStatus.c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Invoices")) {
            businessTab = 1;
            ImGui::SetNextItemWidth(220);
            ImGui::InputText("Label", invLabel, sizeof(invLabel));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::InputText("Amount", invAmount, sizeof(invAmount));
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("Note", invNote, sizeof(invNote));
            if (ImGui::Button("Create invoice") && snap.hasWallet)
                CreateBusinessInvoice(invLabel, std::atof(invAmount), invNote);
            ImGui::SameLine();
            ImGui::TextDisabled("Amount 0 = any payment.");
            ImGui::Separator();
            if (ImGui::BeginTable("##inv", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
                                  ImVec2(-1, 220))) {
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Note", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (int i = 0; i < (int)invoices.size(); ++i) {
                    const auto& inv = invoices[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(inv.status.c_str(), invoiceSel == i, ImGuiSelectableFlags_SpanAllColumns))
                        invoiceSel = i;
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(inv.label.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.4f", inv.amount);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(inv.address.c_str());
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(inv.note.c_str());
                }
                ImGui::EndTable();
            }
            if (invoiceSel >= 0 && invoiceSel < (int)invoices.size()) {
                BizInvoice& inv = invoices[invoiceSel];
                if (ImGui::Button("Copy address"))
                    CopyToClipboard(inv.address);
                ImGui::SameLine();
                if (ImGui::Button("Copy dogecoin: URI"))
                    CopyToClipboard(InvoiceUri(inv.address, inv.amount, inv.label));
                ImGui::SameLine();
                if (inv.status == "open" && ImGui::Button("Mark paid")) {
                    inv.status = "paid";
                    SaveBusinessInvoices();
                    businessStatus = "Marked paid: " + inv.label;
                }
                ImGui::SameLine();
                if (inv.status == "open" && ImGui::Button("Cancel")) {
                    inv.status = "cancelled";
                    SaveBusinessInvoices();
                    businessStatus = "Cancelled: " + inv.label;
                }
                ImGui::Dummy(ImVec2(0, 10));
                ImGui::TextDisabled("Scan to pay this invoice");
                DrawQrCode(InvoiceUri(inv.address, inv.amount, inv.label), 180.0f);
            }
            if (!businessStatus.empty())
                ImGui::TextWrapped("%s", businessStatus.c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("POS")) {
            businessTab = 2;
            DrawPosKeypad();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    EndContentPage();
}

void App::ApplyPosDigit(const char* key)
{
    if (!key || !key[0])
        return;
    if (key[0] == '<' || (key[0] == '\x08') || std::strcmp(key, "bk") == 0) {
        if (posBuffer.size() <= 1)
            posBuffer = "0";
        else
            posBuffer.pop_back();
        return;
    }
    if (key[0] == '.') {
        if (posBuffer.find('.') == std::string::npos)
            posBuffer += '.';
        return;
    }
    if (key[0] >= '0' && key[0] <= '9') {
        if (posBuffer == "0")
            posBuffer = key;
        else if (posBuffer.size() < 14)
            posBuffer += key;
    }
}

void App::PosClear()
{
    posBuffer = "0";
}

void App::PosNewSale()
{
    PosClear();
    posAddress.clear();
    posSaleAmount = 0;
    businessStatus = "Ready for a new sale.";
}

void App::PosCharge()
{
    double v = std::atof(posBuffer.c_str());
    if (v < 0) {
        businessStatus = "Invalid amount.";
        return;
    }
    if (CreateBusinessInvoice("POS sale", v, "POS sale")) {
        posAddress = invoices.front().address;
        posSaleAmount = v;
        businessStatus = "Charge ready - customer pays this address.";
    }
}

void App::DrawPosKeypad()
{
    const float padW = 280.0f;
    ImGui::BeginChild("##pospad", ImVec2(padW, 0), true);
    ImGui::TextUnformatted("POS  ·  amount (DOGE)");
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.08f, 0.04f, 1.f));
    ImGui::BeginChild("##posdisp", ImVec2(-1, 64), true);
    ImGui::SetWindowFontScale(1.6f);
    const ImVec2 ts = ImGui::CalcTextSize(posBuffer.c_str());
    ImGui::SetCursorPosX(ImMax(8.0f, ImGui::GetWindowWidth() - ts.x - 16.0f));
    ImGui::SetCursorPosY((64.0f - ts.y) * 0.35f);
    ImGui::TextColored(ImVec4(1.f, 0.85f, 0.25f, 1.f), "%s", posBuffer.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));

    const char* keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "bk"};
    const float gap = 8.0f;
    const float cell = (padW - ImGui::GetStyle().WindowPadding.x * 2 - gap * 2) / 3.0f;
    for (int i = 0; i < 12; ++i) {
        if (i % 3 != 0)
            ImGui::SameLine(0, gap);
        char id[16];
        std::snprintf(id, sizeof(id), "%s##pk%d", keys[i], i);
        const char* label = (std::strcmp(keys[i], "bk") == 0) ? "<" : keys[i];
        if (ImGui::Button(label, ImVec2(cell, 48)))
            ApplyPosDigit(keys[i]);
        (void)id;
    }
    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button("Clear", ImVec2(cell, 44)))
        PosClear();
    ImGui::SameLine(0, gap);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.50f, 0.10f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.65f, 0.16f, 1.f));
    if (ImGui::Button("Charge", ImVec2(cell * 2 + gap, 44)) && snap.hasWallet)
        PosCharge();
    ImGui::PopStyleColor(2);
    if (!snap.hasWallet)
        ImGui::TextDisabled("Wallet RPC needed to Charge.");
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##possale", ImVec2(0, 0), true);
    ImGui::TextUnformatted("Payment request");
    ImGui::TextDisabled("Enter amount and press Charge — customer pays this address.");
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));
    if (posAddress.empty()) {
        ImGui::TextUnformatted("Address:  --");
        ImGui::TextDisabled("Payment request appears after Charge.");
        ImGui::Dummy(ImVec2(0, 12));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float q = 200.0f;
        dl->AddRectFilled(p, ImVec2(p.x + q, p.y + q), IM_COL32(20, 18, 12, 255), 6.0f);
        dl->AddRect(p, ImVec2(p.x + q, p.y + q), IM_COL32(180, 140, 40, 160), 6.0f);
        ImGui::Dummy(ImVec2(q, q));
    } else {
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::TextColored(ImVec4(0.92f, 0.72f, 0.18f, 1.f), "PAY");
        ImGui::SetWindowFontScale(1.8f);
        ImGui::Text("%s  DOGE", posBuffer.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextWrapped("%s", posAddress.c_str());
        ImGui::Dummy(ImVec2(0, 8));
        DrawQrCode(InvoiceUri(posAddress, posSaleAmount, "POS sale"), 200.0f);
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::TextDisabled("Customer scans this QR or pays the address. Auto-watch marks the sale paid.");
    }
    ImGui::Dummy(ImVec2(0, 12));
    const bool have = !posAddress.empty();
    if (!have)
        ImGui::BeginDisabled();
    if (ImGui::Button("Copy address"))
        CopyToClipboard(posAddress);
    ImGui::SameLine();
    if (ImGui::Button("Copy URI"))
        CopyToClipboard(InvoiceUri(posAddress, posSaleAmount, "POS sale"));
    if (!have)
        ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("New sale"))
        PosNewSale();
    if (!businessStatus.empty()) {
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::TextWrapped("%s", businessStatus.c_str());
    }
    ImGui::EndChild();
}

void App::DrawPlaceholder(const char* title, const char* body)
{
    BeginContentPage(title);
    ImGui::TextUnformatted(title);
    ImGui::TextWrapped("%s", body);
    EndContentPage();
}

void App::HandleTitleDrag()
{
    if (!hostWindow)
        return;
#if defined(_WIN32)
    // Windows HTCAPTION / edge resize own the drag. ImGui only starts a fallback
    // move if the native hit-test is unavailable.
    (void)titleDragging;
    return;
#else
    if (!ImGui::IsItemActive())
        return;
    if (ImGui::IsMouseDoubleClicked(0)) {
        if (glfwGetWindowAttrib(hostWindow, GLFW_MAXIMIZED))
            glfwRestoreWindow(hostWindow);
        else
            glfwMaximizeWindow(hostWindow);
        return;
    }
    if (ImGui::IsMouseDragging(0, 0.0f)) {
        int wx = 0, wy = 0;
        glfwGetWindowPos(hostWindow, &wx, &wy);
        ImVec2 d = ImGui::GetIO().MouseDelta;
        glfwSetWindowPos(hostWindow, wx + (int)d.x, wy + (int)d.y);
    }
#endif
}

void App::DrawWindowChrome()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, kCaptionH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.07f, 0.05f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.72f, 0.55f, 0.18f, 0.35f));
    ImGui::Begin("##Caption", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav);

    const float rowY = 4.0f;
    ImGui::SetCursorPosY(rowY);

    if (brandMark.ok()) {
        ImGui::Image((ImTextureID)(intptr_t)brandMark.id, ImVec2(18, 18));
        ImGui::SameLine();
    }

    // Click-to-open — never BeginMenu (hover steals the caption). Larger hit targets.
    if (ImGui::Button("File", ImVec2(48, 0)))
        ImGui::OpenPopup("##filemenu");
    if (ImGui::BeginPopup("##filemenu")) {
        if (ImGui::MenuItem("Import wallet.dat..."))
            BeginImportWallet();
        if (ImGui::MenuItem("Backup wallet.dat..."))
            BeginBackupWallet();
        ImGui::Separator();
        if (ImGui::MenuItem("Hide to tray (leave node running)"))
            RequestHideToTray();
        if (ImGui::MenuItem("Exit (stop node safely)"))
            RequestStopNodeAndExit();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Help", ImVec2(48, 0)))
        ImGui::OpenPopup("##helpmenu");
    if (ImGui::BeginPopup("##helpmenu")) {
        if (ImGui::MenuItem("RPC help catalog", nullptr, false, snap.connected)) {
            showBootSplash = false;
            activeNav = "console";
            helpLoaded = false;
            RefreshHelpCatalog();
            RunConsoleLine("help");
        }
        if (ImGui::MenuItem("Open console")) {
            showBootSplash = false;
            activeNav = "console";
        }
        if (ImGui::MenuItem("Options")) {
            showBootSplash = false;
            activeNav = "settings";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Start local dogecoind"))
            TryStartLocalNode();
        if (ImGui::MenuItem("Show boot splash / reconnect")) {
            offlineMode = false;
            showBootSplash = true;
            InitBootChecklist();
            autoStartTried = false;
            bootStartTime = 0;
            lastProbeTime = 0;
        }
        ImGui::EndPopup();
    }
#if defined(_WIN32)
    ProGuiSetCaptionMenuRight((int)ImGui::GetItemRectMax().x + 6);
#endif

    char statusBuf[80];
    const int tb = DisplayBlocks(snap, cfg.lastKnownHeight);
    const int th = DisplayHeaders(snap, cfg.lastKnownHeaders);
    if (IsTestnet() && !snap.connected)
        std::snprintf(statusBuf, sizeof(statusBuf), "TESTNET  offline");
    else if (IsTestnet() && snap.initialBlockDownload && th > tb && tb >= 0)
        std::snprintf(statusBuf, sizeof(statusBuf), "TESTNET  %d / %d   %.0f%%",
                      tb, th, snap.verificationProgress * 100.0);
    else if (IsTestnet() && snap.connected && tb >= 0)
        std::snprintf(statusBuf, sizeof(statusBuf), "TESTNET  %d blocks", tb);
    else if (IsTestnet() && snap.connected)
        std::snprintf(statusBuf, sizeof(statusBuf), "TESTNET  syncing…");
    else if (!snap.connected)
        std::snprintf(statusBuf, sizeof(statusBuf), "offline");
    else if (snap.initialBlockDownload && th > tb && tb >= 0)
        std::snprintf(statusBuf, sizeof(statusBuf), "%d / %d blocks   %.0f%%",
                      tb, th, snap.verificationProgress * 100.0);
    else if (snap.connected && tb >= 0)
        std::snprintf(statusBuf, sizeof(statusBuf), "%d blocks", tb);
    else if (snap.connected)
        std::snprintf(statusBuf, sizeof(statusBuf), "syncing…");
    else
        std::snprintf(statusBuf, sizeof(statusBuf), "offline");

    const float pad = ImGui::GetStyle().WindowPadding.x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float winW = ImGui::GetWindowWidth();
    const float frameH = ImGui::GetFrameHeight();
    ImGui::SetWindowFontScale(1.12f);
    const ImVec2 statusSz = ImGui::CalcTextSize(statusBuf);
    const float statusW = statusSz.x;
    ImGui::SetWindowFontScale(1.0f);
    const float btnW = 28.0f;
    const float rightW = btnW + gap + btnW + 4.0f;
    const float rightX = winW - pad - rightW;
    const float leftEnd = ImGui::GetCursorPosX();

    ImGui::SetCursorPos(ImVec2(rightX, rowY));
    if (ImGui::Button("_", ImVec2(btnW, 0)) && hostWindow) {
        if (cfg.minimizeToTray)
            RequestHideToTray();
        else
            glfwIconifyWindow(hostWindow);
    }
#if defined(_WIN32)
    ProGuiSetCaptionButtonLeft((int)ImGui::GetItemRectMin().x - 6);
#endif
    ImGui::SameLine();
    if (ImGui::Button("x", ImVec2(btnW, 0)))
        RequestHideToTray();

    float dragW = rightX - leftEnd - gap;
    if (dragW < 16.0f)
        dragW = 16.0f;
    ImGui::SetCursorPos(ImVec2(leftEnd + 4.0f, rowY));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::InvisibleButton("##captiondrag", ImVec2(dragW, frameH));
    ImGui::PopStyleColor(3);
    const ImVec2 dragMin = ImGui::GetItemRectMin();
    const ImVec2 dragMax = ImGui::GetItemRectMax();
#if defined(_WIN32)
    ProGuiSetCaptionDragRect((int)dragMin.x, (int)dragMin.y, (int)dragMax.x, (int)dragMax.y);
#endif
    HandleTitleDrag();

    float statusX = (winW - statusW) * 0.5f;
    if (statusX < leftEnd + 8.0f)
        statusX = leftEnd + 8.0f;
    if (statusX + statusW > rightX - 8.0f)
        statusX = rightX - 8.0f - statusW;
    float statusY = (kCaptionH - statusSz.y) * 0.5f - 4.0f;
    if (statusY < 1.0f)
        statusY = 1.0f;
    ImGui::SetCursorPos(ImVec2(statusX, statusY));
    ImGui::SetWindowFontScale(1.12f);
    if (!snap.connected)
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "%s", statusBuf);
    else if (snap.initialBlockDownload)
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.25f, 1.f), "%s", statusBuf);
    else
        ImGui::TextColored(ImVec4(0.55f, 1.f, 0.45f, 1.f), "%s", statusBuf);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(5);
}

void App::DrawMainShell()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float menuH = kCaptionH;

    // Fixed left nav rail — always full height under menu, never overlapped
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + menuH));
    ImGui::SetNextWindowSize(ImVec2(kRailW, vp->WorkSize.y - menuH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));
    ImGui::Begin("##Rail", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    DrawSidebar();
    ImGui::End();
    ImGui::PopStyleVar(3);

    // Content is always the remaining full page (BeginContentPage positions it)
    if (activeNav == "home")
        DrawHome();
    else if (activeNav == "network")
        DrawNetwork();
    else if (activeNav == "blocks")
        DrawChain();
    else if (activeNav == "arcade")
        DrawArcade();
    else if (activeNav == "settings")
        DrawOptions();
    else if (activeNav == "send" || activeNav == "receive")
        DrawSendReceive();
    else if (activeNav == "tx")
        DrawHistory();
    else if (activeNav == "console")
        DrawConsole();
    else if (activeNav == "meme")
        DrawMemeStream();
    else if (activeNav == "business")
        DrawBusiness();
    else if (activeNav == "mining")
        DrawMining();

    if (showDemo)
        ImGui::ShowDemoWindow(&showDemo);
}

void App::Frame()
{
    TickTray();
    DrawTrayHintModal();
    if (hiddenInTray)
        return;

    TickWalletOp();
    TickFastSync();
    TickBusinessWatch();
    TickMemeStream();
    DrawWindowChrome();

    if (shuttingDown) {
        arcadeWeb.Show(false);
        DrawShutdownSplash();
        return;
    }

    if (showBootSplash) {
        arcadeWeb.Show(false);
        DrawBootSplash();
        return;
    }

    // Live status while shell is up
    TickNodeProbe();
    DrawMainShell();
    DrawNavFlyout();
    // Position/show WebView2 over Arcade content rect (native HWND, not ImGui)
    UpdateArcadeEmbed();
}
