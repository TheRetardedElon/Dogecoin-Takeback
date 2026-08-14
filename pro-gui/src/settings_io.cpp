#include "settings_io.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static std::string Trim(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}

static bool ParseBool(const std::string& v, bool def)
{
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return def;
}

bool LoadSettings(const std::string& path, ProGuiSettings& out)
{
    std::ifstream f(path);
    if (!f) return false;
    bool hadFirstRunKey = false;
    std::string line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq));
        std::string v = Trim(line.substr(eq + 1));
        auto setS = [&](const char* name, std::string& dest) {
            if (k == name) dest = v;
        };
        auto setI = [&](const char* name, int& dest) {
            if (k == name) dest = std::atoi(v.c_str());
        };
        auto setB = [&](const char* name, bool& dest) {
            if (k == name) dest = ParseBool(v, dest);
        };
        setS("rpcHost", out.rpcHost);
        setI("rpcPort", out.rpcPort);
        setS("network", out.network);
        setS("rpcUser", out.rpcUser);
        setS("rpcPassword", out.rpcPassword);
        setS("cookiePath", out.cookiePath);
        setB("preferFastSync", out.preferFastSync);
        setB("prune", out.prune);
        setI("pruneSizeGb", out.pruneSizeGb);
        setI("dbCacheMb", out.dbCacheMb);
        setS("snapshotUrl", out.snapshotUrl);
        setS("snapshotSha256", out.snapshotSha256);
        setB("startAtLogin", out.startAtLogin);
        setB("spendZeroConfChange", out.spendZeroConfChange);
        setB("coinControl", out.coinControl);
        setB("listen", out.listen);
        setB("upnp", out.upnp);
        setB("dnsSeed", out.dnsSeed);
        setB("proxyEnabled", out.proxyEnabled);
        setS("proxyIp", out.proxyIp);
        setI("proxyPort", out.proxyPort);
        setB("proxyTorEnabled", out.proxyTorEnabled);
        setS("proxyTorIp", out.proxyTorIp);
        setI("proxyTorPort", out.proxyTorPort);
        setB("p2pViaTor", out.p2pViaTor);
        setI("maxConnections", out.maxConnections);
        setI("theme", out.theme);
        setB("minimizeToTray", out.minimizeToTray);
        setB("showTrayNotifications", out.showTrayNotifications);
        setB("trayHintDontShowAgain", out.trayHintDontShowAgain);
        setS("displayUnit", out.displayUnit);
        setS("datadirHint", out.datadirHint);
        setS("archivePath", out.archivePath);
        setS("snapshotDestPath", out.snapshotDestPath);
        setS("homePanels", out.homePanels);
        setI("lastKnownHeight", out.lastKnownHeight);
        setI("lastKnownHeaders", out.lastKnownHeaders);
        setS("memeAuthor", out.memeAuthor);
        setS("hybridDefaultUi", out.hybridDefaultUi);
        setB("preferMdbx", out.preferMdbx);
        if (k == "firstRunDone") {
            hadFirstRunKey = true;
            out.firstRunDone = ParseBool(v, out.firstRunDone);
        }
    }
    // File existed: if the key is absent, this is an older install — skip Intro.
    if (!hadFirstRunKey)
        out.firstRunDone = true;
    return true;
}

bool SaveSettings(const std::string& path, const ProGuiSettings& in)
{
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << "# dogecoin-pro-gui settings (Path A control plane)\n";
    auto w = [&](const char* k, const std::string& v) { f << k << "=" << v << "\n"; };
    auto wi = [&](const char* k, int v) { f << k << "=" << v << "\n"; };
    auto wb = [&](const char* k, bool v) { f << k << "=" << (v ? "1" : "0") << "\n"; };
    w("rpcHost", in.rpcHost);
    wi("rpcPort", in.rpcPort);
    w("network", in.network.empty() ? "main" : in.network);
    w("rpcUser", in.rpcUser);
    // Never persist rpcpassword in the GUI ini. Auth is cookie or dogecoin.conf.
    w("rpcPassword", "");
    w("cookiePath", in.cookiePath);
    wb("preferFastSync", in.preferFastSync);
    wb("prune", in.prune);
    wi("pruneSizeGb", in.pruneSizeGb);
    wi("dbCacheMb", in.dbCacheMb);
    w("snapshotUrl", in.snapshotUrl);
    w("snapshotSha256", in.snapshotSha256);
    wb("startAtLogin", in.startAtLogin);
    wb("spendZeroConfChange", in.spendZeroConfChange);
    wb("coinControl", in.coinControl);
    wb("listen", in.listen);
    wb("upnp", in.upnp);
    wb("dnsSeed", in.dnsSeed);
    wb("proxyEnabled", in.proxyEnabled);
    w("proxyIp", in.proxyIp);
    wi("proxyPort", in.proxyPort);
    wb("proxyTorEnabled", in.proxyTorEnabled);
    w("proxyTorIp", in.proxyTorIp);
    wi("proxyTorPort", in.proxyTorPort);
    wb("p2pViaTor", in.p2pViaTor);
    wi("maxConnections", in.maxConnections);
    wi("theme", in.theme);
    wb("minimizeToTray", in.minimizeToTray);
    wb("showTrayNotifications", in.showTrayNotifications);
    wb("trayHintDontShowAgain", in.trayHintDontShowAgain);
    w("displayUnit", in.displayUnit);
    w("datadirHint", in.datadirHint);
    w("archivePath", in.archivePath);
    w("snapshotDestPath", in.snapshotDestPath);
    wb("firstRunDone", in.firstRunDone);
    wb("preferMdbx", in.preferMdbx);
    w("homePanels", in.homePanels);
    wi("lastKnownHeight", in.lastKnownHeight);
    wi("lastKnownHeaders", in.lastKnownHeaders);
    w("memeAuthor", in.memeAuthor);
    w("hybridDefaultUi", in.hybridDefaultUi);
    return true;
}

static std::string HybridUiFile(const std::string& installDir)
{
    if (installDir.empty())
        return "hybrid-ui.txt";
    char last = installDir.back();
    if (last == '/' || last == '\\')
        return installDir + "hybrid-ui.txt";
    return installDir + "/hybrid-ui.txt";
}

std::string LoadHybridUiPref(const std::string& installDir)
{
    std::ifstream f(HybridUiFile(installDir));
    if (!f) return "ask";
    std::string line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line == "gfx" || line == "gui" || line == "imgui" || line == "desktop") return "gfx";
        if (line == "tui" || line == "operator" || line == "gpenode") return "tui";
        if (line == "ask" || line == "prompt" || line == "choose") return "ask";
    }
    return "ask";
}

static std::string ReadRoleFile(const std::string& installDir);

std::string LoadInstallRole(const std::string& installDir)
{
    return ReadRoleFile(installDir);
}

static std::string DefaultNodeDatadir()
{
#if defined(_WIN32)
    if (const char* a = std::getenv("APPDATA"))
        return std::string(a) + "\\Dogecoin";
#else
    if (const char* h = std::getenv("HOME"))
        return std::string(h) + "/.dogecoin";
#endif
    return {};
}

static bool ParseConfRpc(const std::string& path, std::string& user, std::string& pass, int* port)
{
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    bool got = false;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq));
        std::string v = Trim(line.substr(eq + 1));
        for (char& c : k) {
            if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        }
        if (k == "rpcuser" && !v.empty()) { user = v; got = true; }
        else if (k == "rpcpassword" && !v.empty()) { pass = v; got = true; }
        else if (k == "rpcport" && port && !v.empty()) *port = std::atoi(v.c_str());
    }
    return got && !user.empty() && !pass.empty();
}

bool LoadNodeRpcCredentials(const std::string& datadirHint,
                            std::string& user, std::string& pass, int* port)
{
    std::vector<std::string> dirs;
    if (!datadirHint.empty()) dirs.push_back(datadirHint);
    std::string def = DefaultNodeDatadir();
    if (!def.empty()) dirs.push_back(def);
    for (const auto& d : dirs) {
        std::string sep = (d.find('\\') != std::string::npos) ? "\\" : "/";
        if (ParseConfRpc(d + sep + "dogecoin.conf", user, pass, port))
            return true;
        if (ParseConfRpc(d + sep + "RPC-CREDENTIALS.txt", user, pass, port))
            return true;
    }
    return false;
}

bool SaveHybridUiPref(const std::string& installDir, const std::string& value)
{
    std::string v = value;
    if (v != "gfx" && v != "tui" && v != "ask")
        v = "ask";
    std::ofstream f(HybridUiFile(installDir), std::ios::trunc);
    if (!f) return false;
    f << "# Hybrid default UI for this install. ask | gfx | tui\n";
    f << v << "\n";
    return true;
}

static std::string JoinInstall(const std::string& installDir, const char* name)
{
    if (installDir.empty())
        return name;
    char last = installDir.back();
    if (last == '/' || last == '\\')
        return installDir + name;
    return installDir + "/" + name;
}

static std::string ReadRoleFile(const std::string& installDir)
{
    std::ifstream f(JoinInstall(installDir, "install-role.txt"));
    if (!f) return "";
    std::string line;
    if (!std::getline(f, line)) return "";
    line = Trim(line);
    for (char& c : line) {
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    }
    return line;
}

bool HybridHasExplicitUiFlag(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i] ? argv[i] : "";
        if (a == "--ui" && i + 1 < argc) {
            std::string v = argv[i + 1] ? argv[i + 1] : "";
            if (v == "gfx" || v == "tui" || v == "gui" || v == "ask")
                return true;
        }
        if (a.rfind("--ui=", 0) == 0)
            return true;
    }
    return false;
}

bool HybridShouldPrompt(const std::string& installDir)
{
    std::string role = ReadRoleFile(installDir);
    if (role != "hybrid")
        return false;
    return LoadHybridUiPref(installDir) == "ask";
}

bool RelaunchHybridPicker(const std::string& installDir)
{
#if defined(_WIN32)
    std::string dir = installDir;
    std::string launch = JoinInstall(installDir, "corepro-launch.exe");
    std::ifstream chk(launch);
    if (!chk) return false;
    chk.close();
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(launch.begin(), launch.end());
    buf.push_back('\0');
    if (!CreateProcessA(launch.c_str(), buf.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    (void)installDir;
    return false;
#endif
}

bool ExportNodeConfFragment(const std::string& path, const ProGuiSettings& in)
{
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << "# Generated by dogecoin-pro-gui — merge into dogecoin.conf or use -conf=\n";
    f << "# Restart dogecoind after changing these.\n\n";
    f << "server=1\n";
    f << "listen=" << (in.listen ? "1" : "0") << "\n";
    f << "upnp=" << (in.upnp ? "1" : "0") << "\n";
    f << "dnsseed=" << (in.dnsSeed ? "1" : "0") << "\n";
    f << "maxconnections=" << in.maxConnections << "\n";
    f << "dbcache=" << in.dbCacheMb << "\n";
    if (in.preferMdbx)
        f << "dbengine=mdbx\n";
    else
        f << "dbengine=leveldb\n";
    if (in.prune) {
        int pruneMb = in.pruneSizeGb * 1000; // approximate GB -> MiB target like Core
        if (pruneMb < 550) pruneMb = 5500;
        f << "prune=" << pruneMb << "\n";
    } else {
        f << "prune=0\n";
    }
    f << "spendzeroconfchange=" << (in.spendZeroConfChange ? "1" : "0") << "\n";
    if (in.p2pViaTor || in.proxyEnabled) {
        const std::string ip = in.p2pViaTor ? "127.0.0.1" : in.proxyIp;
        const int port = in.p2pViaTor ? 9050 : in.proxyPort;
        f << "proxy=" << ip << ":" << port << "\n";
        if (in.p2pViaTor)
            f << "listenonion=0\n";
    }
    if (!in.p2pViaTor && in.proxyTorEnabled) {
        f << "onion=" << in.proxyTorIp << ":" << in.proxyTorPort << "\n";
    }
    if (!in.snapshotUrl.empty()) {
        f << "# Core Pro Fast Sync (if supported by your build)\n";
        f << "# snapshoturl=" << in.snapshotUrl << "\n";
        if (!in.snapshotSha256.empty())
            f << "# snapshotsha256=" << in.snapshotSha256 << "\n";
    }
    if (!in.datadirHint.empty()) {
        f << "# datadir=" << in.datadirHint << "\n";
    }
    if (!in.archivePath.empty()) {
        f << "archivepath=" << in.archivePath << "\n";
    }
    if (!in.snapshotDestPath.empty()) {
        f << "# snapshotdest=" << in.snapshotDestPath << "\n";
    }
    return true;
}

bool UpsertNodeConfKey(const std::string& confPath, const std::string& key, const std::string& value)
{
    if (confPath.empty() || key.empty())
        return false;
    std::vector<std::string> lines;
    {
        std::ifstream in(confPath);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
    }
    const std::string prefix = key + "=";
    bool replaced = false;
    for (auto& line : lines) {
        std::string t = Trim(line);
        if (t.rfind(prefix, 0) == 0 || t.rfind("#" + prefix, 0) == 0 ||
            t.rfind("# " + prefix, 0) == 0) {
            line = prefix + value;
            replaced = true;
        }
    }
    if (!replaced)
        lines.push_back(prefix + value);
    std::ofstream out(confPath, std::ios::trunc);
    if (!out)
        return false;
    for (size_t i = 0; i < lines.size(); ++i) {
        out << lines[i] << "\n";
    }
    return (bool)out;
}
