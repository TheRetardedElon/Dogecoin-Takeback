#pragma once

#include <string>
#include <vector>

/** Locate dogecoind / dogecoind.exe near the GUI, release/, or install dirs. */
std::vector<std::string> FindDogecoindCandidates(const std::string& assetsRoot);

/** Best single candidate path (empty if none found). Absolute when possible. */
std::string FindBestDogecoind(const std::string& assetsRoot);

/**
 * Start dogecoind with Core Pro defaults:
 *   -server  (RPC)
 *   -prune=<pruneMiB>  when pruneMiB > 0 (5500 if datadir was previously pruned —
 *                 without this, AppInit aborts and RPC port vanishes → splash Step 2 forever)
 *   -dbcache=<dbCacheMb> when dbCacheMb > 0
 * Optional -datadir.
 */
bool StartDogecoind(const std::string& exePath, const std::string& datadirHint, std::string& errOut,
                    int pruneMiB = 5500, int dbCacheMb = 0,
                    const std::string& archivePath = "",
                    const std::string& dbEngine = "",
                    const std::string& extraArgs = "");

/** True if a process named dogecoind is running (best-effort Windows). */
bool IsDogecoindRunning();

#if defined(_WIN32)
enum class CoreProServiceState { Missing, Stopped, Running, Other };

CoreProServiceState QueryCoreProService();
/** Start Windows service DogecoinGPENode (Hybrid/Server). */
bool StartCoreProService(std::string& errOut);
/** SCM stop and wait until STOPPED (does not RPC-stop; caller should). */
bool StopCoreProServiceWait(int waitMs, std::string& errOut);
/** RPC-stop, wait for process, then SCM-stop, then SCM-start. */
bool RestartCoreProNode(const std::string& host, int port,
                        const std::string& cookiePath,
                        const std::string& user, const std::string& pass,
                        std::string& errOut);
#endif

/** Absolute path of dogecoin-pro-gui / smoke exe next to us, or empty. */
std::string FindProGuiExe();

/** Launch the ImGui shell (absolute path). */
bool StartProGui(const std::string& guiPath, std::string& errOut);

/** Make absolute path if relative. */
std::string MakeAbsolutePath(const std::string& path);

/**
 * Graceful node stop (Path A product exit):
 * Prefer RPC `stop` so dogecoind flushes wallets/chainstate (same intent as Qt
 * StartShutdown). Caller should poll IsDogecoindRunning() until false.
 * Returns false only if stop could not be requested at all.
 */
bool RequestDogecoindStop(const std::string& host, int port,
                          const std::string& cookiePath,
                          const std::string& user, const std::string& pass,
                          std::string& errOut);

/** Best-effort default debug.log path (Windows %APPDATA%\\Dogecoin\\debug.log). */
std::string DefaultDebugLogPath(const std::string& network = "main");

/** Read tail of debug.log and return last matching InitMessage-style line (may be empty). */
std::string TailInitMessageFromDebugLog(const std::string& logPath);
