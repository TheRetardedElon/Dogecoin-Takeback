#pragma once

#include "arcade_webview.h"
#include "memestream_http.h"
#include "rpc_client.h"
#include "rpc_probe.h"
#include "settings_io.h"
#include "texture.h"
#include "theme.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct GLFWwindow;

struct NavItem {
    std::string id;
    std::string label;
    std::string file;
    Texture tex;
};

/** Checklist row for boot / shutdown splash (Qt-parity informing). */
struct CheckItem {
    std::string id;
    std::string label;
    enum class State { Pending, Active, Done, Fail, Skip } state = State::Pending;
};

/**
 * Path A control plane: dogecoind owns consensus/wallet DB;
 * this process is ImGui + RPC. Product entry owns start + stop of the node
 * the same way Qt owns in-process AppInit / StartShutdown.
 */
class App {
public:
    bool Init(const std::string& assetsDir, int argc = 0, char** argv = nullptr);
    bool IsTestnet() const;
    void Shutdown(); // save settings only — call after node stop completes
    void Frame();
    /** GLFW window for native embeds (WebView2 arcade). */
    void SetHostWindow(GLFWwindow* window);

    /** Caption X / window close: hide to tray (node stays up). */
    void RequestHideToTray();
    void RestoreFromTray();
    void TickTray();
    /** Begin shutdown. Stops the node unless Hybrid/Server left stopNodeOnExit off
     *  and this was not a forced File→Exit / tray Quit. */
    void RequestExit();
    /** File→Exit / tray Quit: stop Windows service (if any), RPC-stop dogecoind,
     *  wait for flush, then exit the GUI. */
    void RequestStopNodeAndExit();
    bool IsShuttingDown() const { return shuttingDown; }
    bool IsShutdownComplete() const { return shutdownComplete; }
    bool wantsClose = false; // true when process may exit

private:
    void UpdateArcadeEmbed();
    void LoadAssets(const std::string& assetsDir);
    void LoadOrInitSettings();
    void ApplySettingsToUi();
    void CollectUiToSettings();
    void SyncRpcConfig();
    void ApplyRpcFromNodeConf();
    std::string InstallDir() const;
    void TickBoot();
    void TickShutdown();
    void TickNodeProbe();
    void RefreshHelpCatalog();
    void RunConsoleLine(const std::string& line);
    void InitBootChecklist();
    void SetCheck(const char* id, CheckItem::State st, const char* labelOverride = nullptr);
    void AdvanceBootFromSignals();
    bool TryStartLocalNode();
    int LaunchPruneMiB() const;
    void ApplyDefaultDatadirHint();
    bool NeedsFirstRunPrompt() const;
    void FinishFirstRunAndStart();
    void BeginContentPage(const char* title);
    void EndContentPage();

    void DrawWindowChrome();
    void HandleTitleDrag();
    void TickWalletOp();
    void BeginImportWallet();
    void BeginBackupWallet();
    void FinishImportWallet();
    void TickFastSync();
    void StartResolveManifest();
    void StartDownloadSnapshot();
    void StartLoadSnapshot(bool activate);
    void StartArchiveRpc(const char* method, const std::string& paramsJson = "[]");
    void ApplyOfficialFastSyncDefaults();
    std::string WalletDatPath() const;

    void DrawBootSplash();
    void DrawShutdownSplash();
    void DrawSidebar();
    void DrawMainShell();
    void DrawHome();
    void DrawHomePanel(const std::string& id, int index);
    void DrawNavFlyout();
    void DrawNetwork();
    void DrawChain();
    void DrawArcade();
    void DrawMemeStream();
    void DrawMemeCard(const MemeItem& item, bool compact);
    void DrawMemeRail();
    void TickMemeStream();
    void StartMemeFeed();
    void EnsureMemeAuthor();
    void TipMemeCreator(const std::string& address);
    void StartMemePublish();
    void StartMemeLike(const std::string& id);
    void PickMemeImage();
    void ClearMemeImage();
    void DrawBusiness();
    void DrawSendReceive();
    void DrawHistory();
    void DrawConsole();
    void DrawOptions();
    void ApplyP2pTorPref(bool on);
    void ParseLaunchArgs(int argc, char** argv);
    void ApplyNetworkMode();
    void DrawMining();
    void DrawPlaceholder(const char* title, const char* body);
    void LoadBusinessInvoices();
    void SaveBusinessInvoices() const;
    void TickBusinessWatch();
    bool CreateBusinessInvoice(const std::string& label, double amount, const std::string& note);
    std::string InvoiceUri(const std::string& address, double amount, const std::string& label) const;
    std::string InvoicesPath() const;
    void OpenArcadeHub();
    void OpenGameUrl(const std::string& url);
    void CopyToClipboard(const std::string& text);
    void RefreshHistory();
    void ParseHistoryJson(const std::string& json);
    void DrawWorldPeerMap(float width, float height);

    std::string assetsRoot;
    std::string dogecoindPath;
    std::string nodeLaunchStatus;
    bool nodeLaunchAttempted = false;
    bool weStartedNode = false; // true if this session spawned dogecoind
    bool stopNodeOnExit = true; // product default: clean stop like Qt
    bool forceStopNodeOnExit = false;
    bool rpcStuckRestartTried = false;
    double lastHeightSaveTime = 0.0;
    RpcProbeWorker probeWorker;
    std::string settingsPath;
    std::string nodeConfExportPath;
    std::string activeNav = "home";
    ProTheme theme = ProTheme::GoldDark;

    std::vector<NavItem> nav;
    std::map<std::string, Texture> icons;

    Texture splash;
    Texture brandMark;
    Texture worldMap; // equirectangular basemap for peer plot

    // --- Boot (product splash checklist) ---
    bool showBootSplash = true;
    bool offlineMode = false;
    bool autoStartTried = false;
    double bootStartTime = 0.0;
    double readySinceTime = 0.0;
    double lastProbeTime = 0.0;
    double lastLogPoll = 0.0;
    std::string bootHeadline = "WHO LET THE DOGE OUT?!";
    std::string bootDetail;
    std::string lastInitMessage; // from debug.log (core InitMessage)
    std::vector<CheckItem> bootChecks;

    // --- Shutdown (Qt ShutdownWindow parity) ---
    bool shuttingDown = false;
    bool shutdownComplete = false;
    bool stopRequested = false;
    double shutdownStartTime = 0.0;
    std::string shutdownHeadline = "Dogecoin Core is shutting down...";
    std::string shutdownDetail;
    std::vector<CheckItem> shutdownChecks;

    RpcClient rpc;
    NodeSnapshot snap;
    ProGuiSettings cfg;

    char rpcHost[128] = "127.0.0.1";
    int rpcPort = 22555;
    char rpcUser[64] = "";
    char rpcPass[128] = "";
    char cookiePath[512] = "";
    char encryptPass1[128] = "";
    char encryptPass2[128] = "";
    char snapshotUrl[512] = "";
    char snapshotSha[128] = "";
    char proxyIp[64] = "127.0.0.1";
    char proxyTorIp[64] = "127.0.0.1";
    char datadirHint[512] = "";
    char archivePath[512] = "";
    char snapshotDestPath[512] = "";
    char defaultDatadir[512] = "";
    bool useCustomDatadir = false;
    std::string firstRunError;
    char displayUnit[16] = "DOGE";

    char consoleInput[512] = "help";
    char consoleParams[512] = "[]";
    std::string consoleOut;
    std::vector<std::string> consoleHistory;
    int consoleHistoryIdx = -1;
    std::vector<std::string> helpCommands;
    char helpFilter[64] = "";
    std::string helpDetail;
    bool helpLoaded = false;

    int sendReceiveTab = 0; // 0 = Send, 1 = Receive
    char sendAddr[128] = "";
    char sendAmount[32] = "0";
    char sendComment[128] = "";
    std::string sendStatus;
    char receiveAddr[128] = "";
    char receiveLabel[64] = "";
    std::string receiveStatus;

    struct TxRow {
        std::string category;
        std::string address;
        std::string label;
        std::string txid;
        double amount = 0.0;
        int confirmations = 0;
        long long time = 0;
    };
    std::vector<TxRow> historyRows;
    std::string historyStatus;
    bool historyLoaded = false;
    double lastHistoryFetch = 0.0;

    char snapshotPath[512] = "";
    char snapshotManifest[512] = "https://sync.doge.gopastearth.com/latest.json";
    char snapshotShaArg[128] = "";
    std::string fastSyncStatus;
    std::string fastSyncArtifactUrl;
    std::string fastSyncHeight;
    std::string fastSyncBytes;
    std::atomic<bool> fastSyncBusy{false};
    mutable std::mutex fastSyncMu;
    std::thread fastSyncThread;
    bool pendingManifestReady = false;
    std::string pendingManifestBody;

    enum class WalletOp { None, ImportWaitStop };
    WalletOp walletOp = WalletOp::None;
    char walletImportSrc[512] = "";
    std::string walletStatus;
    double walletOpStart = 0.0;

    bool titleDragging = false;
    int titleDragOffX = 0;
    int titleDragOffY = 0;

    static constexpr float kCaptionH = 42.0f;

    std::string arcadeHubUrl = "https://arcade.gopastearth.com/";
    std::string arcadeHubAlt = "https://gopastearth.com/arcade";
    std::string memeStreamUrl = "https://memestream.gopastearth.com";
    std::string embedNavUrl;
    bool memeWebOnHome = false;
    int memeWebX = 0, memeWebY = 0, memeWebW = 0, memeWebH = 0;
    int memeTab = 0;
    char memeTipAddr[128] = "";
    char memeTipAmt[32] = "1";
    std::string memeStatus;
    char memeSearch[80] = "";
    char memePubTitle[160] = "";
    char memePubBody[512] = "";
    char memeAuthor[128] = "";
    std::string memeImageName;
    std::string memeImageBytes;
    std::string memeImageMime;
    Texture memeImagePreview;
    std::vector<MemeItem> memeItems;
    MemeItem memeOfTheDay;
    std::string memeFeedStatus;
    std::map<std::string, Texture> memeThumbs;
    std::atomic<bool> memeBusy{false};
    std::atomic<unsigned> memeJobGen{0};
    std::thread memeThread;
    std::mutex memeMu;
    bool memeFeedDirty = false;
    std::vector<MemeItem> memeItemsPending;
    MemeItem memeOfTheDayPending;
    std::string memePendingStatus;
    std::string memePendingError;
    std::map<std::string, std::string> memePendingImages;
    std::string memePendingLikeId;
    double lastMemeFetch = 0;

    struct BizInvoice {
        std::string id;
        std::string label;
        std::string address;
        std::string note;
        std::string status; // open | paid | cancelled
        double amount = 0.0;
        long long created = 0;
    };
    std::vector<BizInvoice> invoices;
    int businessTab = 0;
    int invoiceSel = -1;
    char invLabel[64] = "";
    char invAmount[32] = "0";
    char invNote[128] = "";
    char posAmount[32] = "0";
    std::string posBuffer = "0";
    std::string posAddress;
    double posSaleAmount = 0.0;
    std::string businessStatus;
    void ApplyPosDigit(const char* key);
    void PosClear();
    void PosCharge();
    void PosNewSale();
    void DrawPosKeypad();
    void DrawQrCode(const std::string& payload, float sizePx);
    double lastInvoiceWatch = 0.0;
    bool showDemo = false;
    std::string statusLine = "Waiting for node...";
    int optionsTab = 0;
    std::string optionsStatus;

    static constexpr float kRailW = 72.0f;
    std::string navHoverLabel;
    float navHoverY = 0.0f;

    std::vector<std::string> homePanels;
    void EnsureHomePanels();
    void PersistHomePanels();
    void AddHomePanel(const std::string& id);
    void RemoveHomePanel(int index);
    void MoveHomePanel(int index, int delta);

    GLFWwindow* hostWindow = nullptr;
    ArcadeWebView arcadeWeb;
    bool arcadeNavStarted = false;
    std::string arcadeEmbedStatus;

    bool hiddenInTray = false;
    bool trayHintPending = false;
    void DoHideToTray();
    void DrawTrayHintModal();
};
