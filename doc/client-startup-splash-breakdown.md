# Core Pro client startup & splash — full breakdown

**Purpose:** Document everything that happens from process start until the main wallet UI is usable. The required work is **node** init (`AppInit*`), not a decorative splash.

**Primary sources:** `src/init.cpp`, `src/wallet/wallet.cpp`, plus the historical Qt splash (`src/qt/dogecoin.cpp`, `splashscreen.cpp`, `intro.cpp`) and the shipped ImGui splash (`pro-gui/src/app.cpp`).

**Scope:** 1.14.104 shipping desktop is `dogecoin-pro-gui` (Path A): attach to a running `dogecoind` or start it, then wait on localhost RPC. The GUI must **not** run `AppInitMain` on the render thread. Headless `dogecoind` still owns `AppInit*`. The Qt thread diagram below is the historical in-process splash; ImGui replaced it with an RPC-wait checklist.

---

## Big picture (two threads)

```
UI THREAD                              CORE WORKER THREAD
─────────                              ──────────────────
Parse CLI, Qt app, translations
Intro (datadir) if needed
Read conf, SelectParams
Create splash (show)
Create main window (hidden-ish)
requestInitialize  ──────────────────► AppInitBasicSetup
  (process events, paint splash)       AppInitParameterInteraction
  show InitMessage strings             AppInitSanityChecks
                                       AppInitMain  ← long
initializeResult ◄──────────────────── success/fail
  ClientModel, WalletModel
  show main window
  splashFinished → hide splash
  maybeOfferFastSync (timer 1.5s)
app.exec() event loop
```

**Rule:** Splash is *not* decorative only. It exists so the UI thread can process paint/events while the **core thread** does disk/crypto work that would freeze a single-threaded GUI.

---

## PHASE 0 — Process entry (before any splash)

These run on the **main/UI thread** and **must** happen before a meaningful splash that talks about “loading the node.”

### 0.1 Environment & CLI
1. `SetupEnvironment()` — locale, filesystem, etc.
2. `ParseParameters(argc, argv)` — CLI overrides everything later
3. **Do not** call `GetDataDir(true)` yet — Intro may change datadir

### 0.2 Qt / GUI toolkit bootstrap (Qt-specific)
4. `Q_INIT_RESOURCE(dogecoin)` / `dogecoin_locale`
5. HiDPI attributes (`AA_UseHighDpiPixmaps`, `AA_EnableHighDpiScaling`)
6. Construct `QApplication` / `DogecoinApplication`
7. Register Qt meta types (`bool*`, `CAmount`)
8. Org/app identity for `QSettings` (`QAPP_ORG_NAME`, domain, app name)
9. Font substitution for locale
10. Load translations (system → QSettings → `-lang`)
11. Connect `translationInterface` for core strings

### 0.3 Early exits (no splash)
12. `-?` / `-h` / `-help` / `-version` → help dialog → exit

### 0.4 Data directory (Intro — still pre-splash)
13. `Intro::pickDataDirectory()`:
    - If `-datadir` on CLI → skip dialog
    - Else default OS datadir, then `QSettings strDataDir`
    - If missing / `-choosedatadir` / reset → **Intro dialog**:
      - User picks path
      - Disk free-space checks
      - **Prefer Fast Sync** checkbox (Core Pro):
        - SoftSet `-prune=5500`
        - `fPreferFastSync`, prune keys in QSettings
        - `fPendingFastSyncOffer = true` (modal **after** main UI is up)
    - SoftSet `-datadir` if non-default
14. Fail if datadir is not a directory
15. `ReadConfigFile(dogecoin.conf)` — parse errors are fatal dialogs

### 0.5 Network selection
16. `SelectParams(ChainNameFromCommandLine())` — main / test / regtest  
    - After this, `Params()` is valid
17. Wallet: `PaymentServer::ipcParseCommandLine` (can affect network)
18. `NetworkStyle` → set network-specific app name → **re-init translations**
19. Wallet: URI IPC send to existing instance → may **exit** if another Core has the datadir
20. Create `PaymentServer` early (URI routing)

### 0.6 GUI pre-init (still can be pre-splash)
21. Tooltip event filter, Win session filter
22. `qInstallMessageHandler` → route Qt logs to debug.log category
23. `parameterSetup()`:
    - `InitLogging()`
    - `InitParameterInteraction()` (arg cross-rules)
24. `createOptionsModel()` — load `QSettings` GUI options (`-resetguisettings` supported)
25. Connect `uiInterface.InitMessage` → log line (and later splash)

### 0.7 Splash appears
26. If `-splash` (default true) and not `-min`:
    - `createSplashScreen(networkStyle)`
    - Show frameless splash pixmap (icon + version + copyright)
    - Splash **subscribes** to:
      - `uiInterface.InitMessage`
      - `uiInterface.ShowProgress`
      - `uiInterface.LoadWallet` → wallet `ShowProgress`

### 0.8 Main window object (created before core finishes)
27. `createWindow(networkStyle)` — constructs `DogecoinGUI` (not fully shown as “ready” yet)
28. Poll timer for shutdown detection (200 ms)
29. `requestInitialize()`:
    - Start `coreThread`
    - Emit `requestedInitialize` → core worker runs AppInit chain

---

## PHASE 1 — Core init on worker thread (splash shows messages)

All of this is what the splash **progress text** is waiting on. Order is intentional.

### 1.A `AppInitBasicSetup()`
30. Windows DEP / CRT noise suppression
31. `SetupNetworking()`
32. Unix: umask, SIGTERM/SIGINT/SIGHUP/SIGPIPE handlers
33. `set_new_handler` for OOM

### 1.B `AppInitParameterInteraction()`
34. Prune vs txindex incompatibility
35. File descriptor limits / `-maxconnections` clamping
36. Map debug flags, mempool, script threads, prune mode flags, proxy, etc.
37. Lots of SoftSet/validation of args → internal globals  
    *(anything wrong here = InitError, init fails, splash never “finishes” successfully)*

### 1.C `AppInitSanityChecks()`
38. `RandomInit()`, `ECC_Start()`, verify handle
39. `InitSanityCheck()` — crypto/library self-tests
40. `LockDataDirectory(true)` — **probe** lock (another instance already running?)

### 1.D `AppInitMain()` — the long phase

#### Step 4a — app / logging
41. `LockDataDirectory(false)` — **hold** datadir lock until exit
42. PID file (non-Windows)
43. Optional shrink `debug.log`
44. `OpenDebugLog()` — from here logging is fully real
45. Log paths: datadir, conf, max connections
46. `InitSignatureCache()`
47. Start script-check thread pool
48. Start `CScheduler` service thread
49. If `-server`: start HTTP/RPC in **warmup** mode (accepts connections, rejects most RPCs until ready)

#### Step 5 — wallet DB integrity
50. `CWallet::Verify()` — wallet file integrity (not full rescan yet)

#### Step 6 — network objects (no P2P connect yet)
51. Optional `-asmap` load
52. Construct `CConnman`, `PeerLogicValidation`, register validation interface
53. Bind/listen prep, proxy, banlist load messages, etc.  
    **No outbound block download until later**

#### Step 7 — load chainstate / block index  ★ SLOW
54. Compute `-dbcache` splits (block tree / coins DB / in-memory UTXO)
55. Loop until loaded (or reindex offered):
    - InitMessage: **"Loading block index..."**
    - Open `CBlockTreeDB`, `CCoinsViewDB`, caches
    - Handle `-reindex` / wipe / hardlink blk files if needed
    - `LoadBlockIndex`
    - Genesis check (wrong network datadir?)
    - `InitBlockIndex`
    - txindex / prune consistency checks
    - InitMessage: **"Rewinding blocks..."** if needed
    - InitMessage: **"Verifying blocks..."** + `CVerifyDB`
    - On corruption: ThreadSafeQuestion → reindex or abort
56. **AssumeUTXO / Fast Sync restore (Core Pro):**
    - `InitializeActiveChainstate()`
    - `InitializeBackgroundChainstate()`
    - `MaybeRestoreAssumeUtxo()` — reload snapshot tip from disk if present  
      *(empty wallet + prune + snapshot tip safety hooks apply on later wallet load)*
57. Honor shutdown request if user closed during long load
58. Load fee estimates file if present

#### Step 8 — load wallet  ★ SLOW / can fail on prune
59. `CWallet::InitLoadWallet()`:
    - Open wallet DB
    - Load keys, txs, metadata
    - Rescan logic / best-block locator
    - **Prune gate:** if rescan would go beyond pruned data → InitError  
      - Core Pro: empty wallet + `IsSnapshotChainstateActive()` → anchor tip, skip fatal
    - InitMessage / ShowProgress during rescan: **"Rescanning..."**
60. Register wallet validation interface

#### Step 9 — prune maintenance
61. If prune mode: unset `NODE_NETWORK`, InitMessage **"Pruning blockstore..."**, `PruneAndFlush()`

#### Step 10 — import blocks / genesis
62. Disk space check
63. Wait for genesis (`fHaveGenesis`) — may import `-loadblock` files on import thread
64. Block notify hooks

#### Step 11 — start P2P node
65. Optional Tor control
66. `Discover` local addresses
67. UPnP map
68. `connman.Start(...)` — **now** peers connect, IBD/headers can run
69. Logs: mapBlockIndex size, tip height

#### Step 12 — finished
70. `SetRPCWarmupFinished()` — RPC fully open
71. InitMessage: **"Done loading"**
72. Wallet `postInitProcess` (background tasks)
73. Return success → `initializeResult(true)` on UI thread

---

## PHASE 2 — After splash: wire UI models & show main window

Still UI thread, triggered by `DogecoinApplication::initializeResult`:

74. Create `ClientModel(optionsModel)` — bridges node state to Qt  
75. `window->setClientModel(clientModel)`  
76. If wallet: create `WalletModel`, `addWallet`, `setCurrentWallet`  
77. Show main window (or minimized if `-min`)  
78. **`splashFinished(window)`** → splash `slotFinish` → hide + deleteLater  
79. PaymentServer options + URI/payment request wiring; `uiReady` timer  
80. Main `app.exec()` event loop — **client is “up”**

### Post-splash product hooks (not on splash, but “first run”)
81. ~1.5s later: `maybeOfferFastSync()` if `fPendingFastSyncOffer`  
    - Opens Fast Sync dialog (CDN download / load / activate)  
    - Mid-IBD+prune gate, quiet net, etc. (1.14.103 automation)  
82. User can later open Fast Sync from Settings anytime  

---

## PHASE 3 — What splash messages typically show (user-visible)

| Message / progress | Meaning |
|--------------------|---------|
| Loading addresses… | AddrMan / peers.dat style loads (net) |
| Loading banlist… | Ban list |
| Loading block index… | LevelDB block index + chainstate open |
| Rewinding blocks… | Soft-fork / witness rewind |
| Verifying blocks… | Startup `CVerifyDB` |
| Rescanning… / % | Wallet scan of chain for txs |
| Pruning blockstore… | Initial prune |
| Done loading | Core ready; splash about to close |

(Exact set depends on flags: prune, reindex, wallet, first run, AssumeUTXO restore.)

---

## Required vs optional for a *new* ImGui splash

### REQUIRED for a full in-process wallet+node GUI (like Qt)
Everything in **Phase 0.1, 0.4–0.5, Phase 1 (AppInit*)**, then a model layer equivalent to ClientModel/WalletModel.

Splash must:
- Run on UI/render thread (or dedicated UI thread)
- Drive AppInit on a **background** thread
- Subscribe to `InitMessage` / progress
- Block main chrome until init succeeds
- Fail cleanly (message box / fatal screen) on InitError

### REQUIRED for ImGui-as-control-plane only (recommended architecture)
**Do not re-run full AppInit inside ImGui.** Instead:

| Splash / pre-load job | Owner |
|----------------------|--------|
| Find RPC host/port/user/cookie | ImGui splash |
| Wait until `getblockchaininfo` works (RPC out of warmup) | ImGui splash |
| Optional: start/stop `dogecoind` as child/service | ImGui or OS service |
| Load local assets/themes | ImGui |
| Open arcade hub later | ImGui panel |

Node splash (if you launch daemon yourself) is **dogecoind’s** problem — log “Done loading” / RPC warmup — ImGui just polls.

### OPTIONAL / product-only (can defer)
- Intro Fast Sync preference UI (can be first-run panel after connect)
- Payment URI IPC (Qt-specific)
- Full QSettings theme packs
- Multi-wallet switcher
- Embedding WebView for arcade

### MUST NOT skip if you claim “full node in one binary”
- Datadir lock (single instance)
- ECC / sanity checks
- Load block index + chainstate
- Wallet verify + load (if wallet enabled)
- Genesis wait before “ready”
- Starting connman (or you have no sync)

---

## Failure modes the splash must handle

1. Datadir missing / unreadable / cannot create  
2. Conf parse error  
3. Wrong network for datadir (genesis mismatch)  
4. Datadir already locked (second instance)  
5. Sanity check fail  
6. Corrupt block DB → offer reindex  
7. Prune + wallet rescan beyond data  
8. AssumeUTXO restore failure  
9. Disk full  
10. User cancels Intro  
11. Init fails mid-way → no main UI; process exits  

---

## Mapping to Core Pro features

| Feature | When it appears in startup |
|---------|----------------------------|
| Prefer Fast Sync + prune SoftSet | Intro (Phase 0.4), **before** splash/core |
| AssumeUTXO restore | AppInitMain after block index (Phase 1.D) |
| Fast Sync CDN dialog | **After** main window (~1.5s), not during splash |
| Quiet P2P / mid-IBD gate | Inside Fast Sync worker when user runs it |
| Arcade / Meme Stream / Business | After main UI models exist — pure GUI |

---

## Recommended ImGui product sequence (decision)

**Path A — Control plane (preferred for light/potato):**  
`dogecoind` runs headless (service or sibling process) → ImGui splash = “Connecting to node…” (RPC poll) → main ImGui shell.

**Path B — Monolithic like Qt:**  
ImGui splash = same as Phase 0+1 above with AppInit on worker thread → then ImGui main. Heavier; reimplements Qt’s hardest problem.

**Path C — Hybrid:**  
Installer starts service `dogecoind`; ImGui is always control plane; optional “embedded” mode spawns daemon if RPC dead.

---

## Checklist for Gemini / implementation planning

Copy this checklist:

- [ ] Decide Path A / B / C  
- [ ] List every InitMessage string we want to show  
- [ ] Datadir selection UX (Intro equivalent)  
- [ ] Conf + network selection  
- [ ] Single-instance lock behavior  
- [ ] Worker-thread AppInit OR RPC-wait-only  
- [ ] Progress UI (text + optional bar)  
- [ ] Failure UI for each failure mode  
- [ ] When to enable RPC-driven panels  
- [ ] When to offer Fast Sync (after node ready)  
- [ ] When Arcade hub is allowed (any time after UI; no node dependency)  
- [ ] Shutdown: stop UI timers before tearing RPC; node shutdown separate if multi-process  
- [ ] Never block consensus on render thread  

---

## Key file map

| File | Role |
|------|------|
| `src/qt/dogecoin.cpp` | Qt main sequence, splash create, init thread, initializeResult |
| `src/qt/splashscreen.cpp` | Splash art + InitMessage/ShowProgress subscription |
| `src/qt/intro.cpp` | Datadir + Prefer Fast Sync SoftSets |
| `src/init.cpp` | AppInitBasicSetup / ParameterInteraction / SanityChecks / Main |
| `src/wallet/wallet.cpp` | Verify, load, rescan, prune/AssumeUTXO gates |
| `src/node/chainstate.cpp` | AssumeUTXO restore / dual chainstate |
| `src/qt/dogecoingui.cpp` | maybeOfferFastSync after show |
| `src/qt/fastsyncdialog.cpp` | CDN Fast Sync after UI is up |

---

*Last updated for Core Pro 1.14.103 / ImGui pro-gui planning.*
