#pragma once

#include <string>

/** Simple key=value settings file next to the binary (or assets parent). */
struct ProGuiSettings {
    // Connection
    std::string rpcHost = "127.0.0.1";
    int rpcPort = 22555;
    /** main | test | regtest. test uses testnet3/ and RPC 44555. */
    std::string network = "main";
    std::string rpcUser;
    std::string rpcPassword;
    std::string cookiePath;

    // Main (node conf intent — applied via conf export / restart)
    bool preferFastSync = true;
    bool prune = true;
    int pruneSizeGb = 6;
    int dbCacheMb = 1024;
    /** -par: 0 = auto (default), 1 = single-thread, 2–16 = worker cap. Next node start. */
    int scriptThreads = 0;
    std::string snapshotUrl;
    std::string snapshotSha256;
    bool startAtLogin = false; // GUI note only on Windows

    // Wallet (conf intent)
    bool spendZeroConfChange = true;
    bool coinControl = false;

    // Network (conf intent + some RPC)
    bool listen = true;
    bool upnp = false;
    bool dnsSeed = true;
    bool proxyEnabled = false;
    std::string proxyIp = "127.0.0.1";
    int proxyPort = 9050;
    bool proxyTorEnabled = false;
    std::string proxyTorIp = "127.0.0.1";
    int proxyTorPort = 9050;
    /** Phase 1: dogecoind P2P via local Tor SOCKS. Not WebView. */
    bool p2pViaTor = false;
    int maxConnections = 125;

    // Window / display (GUI)
    int theme = 0; // ProTheme ordinal
    bool minimizeToTray = true; // "_" also hides to tray
    bool showTrayNotifications = true;
    bool trayHintDontShowAgain = false;
    std::string displayUnit = "DOGE";

    // Paths
    std::string datadirHint; // passed as -datadir when non-empty
    // Finalized blk*.dat / snapshot dumps only — cloud or operator file store OK.
    // Not the live datadir. dogecoind -archivepath copies before prune.
    std::string archivePath;
    std::string snapshotDestPath;

    // First-run Intro (Path A). Existing settings files without this key
    // are treated as already configured (see LoadSettings).
    bool firstRunDone = false;
    // Empty/new datadir only. Never flip a live LevelDB folder.
    bool preferMdbx = false;

    // Last IBD tip we saw (resume banner). Not consensus.
    int lastKnownHeight = 0;
    int lastKnownHeaders = 0;

    // Home overview cards, comma-separated ids
    std::string homePanels = "balance,sync,peers,actions";
    // Receive address used as Meme Stream author / tip target
    std::string memeAuthor;

    // Hybrid install only. Shared with launch-hybrid.ps1 and gpenode-tui.
    // ask | gfx | tui  (not a dogecoin.conf key)
    std::string hybridDefaultUi = "ask";
};

/** Read/write INSTDIR/hybrid-ui.txt (ask | gfx | tui). */
std::string LoadHybridUiPref(const std::string& installDir);
bool SaveHybridUiPref(const std::string& installDir, const std::string& value);
bool HybridHasExplicitUiFlag(int argc, char** argv);
bool HybridShouldPrompt(const std::string& installDir);
bool RelaunchHybridPicker(const std::string& installDir);

std::string LoadInstallRole(const std::string& installDir);
/** Fill user/pass/port from installer dogecoin.conf (or RPC-CREDENTIALS.txt). */
bool LoadNodeRpcCredentials(const std::string& datadirHint,
                            std::string& user, std::string& pass, int* port);

bool LoadSettings(const std::string& path, ProGuiSettings& out);
bool SaveSettings(const std::string& path, const ProGuiSettings& in);

/** Write a dogecoin.conf fragment operators can include or merge. */
bool ExportNodeConfFragment(const std::string& path, const ProGuiSettings& in);

/** Set or replace key=value in an existing dogecoin.conf (creates the file if missing). */
bool UpsertNodeConfKey(const std::string& confPath, const std::string& key, const std::string& value);
