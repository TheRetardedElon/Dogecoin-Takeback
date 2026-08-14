// dogeinit — bootstrap before ImGui shell
//
// Order (same as Qt splash responsibility):
//   1) Start dogecoind -server -prune=5500 (so pruned datadirs don't abort)
//   2) Wait until JSON-RPC answers (AppInitMain finished enough for RPC)
//   3) Launch dogecoin-pro-gui / smoke exe
//   4) Exit
//
// This process is intentionally small: no full wallet UI. Splash stages live here
// so the ImGui app only opens when Core is actually usable.

#include "node_launch.h"
#include "rpc_client.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static void Log(const char* msg)
{
    std::printf("%s\n", msg);
    std::fflush(stdout);
#if defined(_WIN32)
    // Also append to dogeinit.log next to exe
    static std::string logPath;
    if (logPath.empty()) {
        char buf[MAX_PATH];
        if (GetModuleFileNameA(nullptr, buf, MAX_PATH)) {
            std::string p(buf);
            auto slash = p.find_last_of("\\/");
            logPath = (slash == std::string::npos) ? "dogeinit.log" : p.substr(0, slash + 1) + "dogeinit.log";
        } else {
            logPath = "dogeinit.log";
        }
    }
    FILE* f = std::fopen(logPath.c_str(), "a");
    if (f) {
        std::fprintf(f, "%s\n", msg);
        std::fclose(f);
    }
#endif
}

static std::string AssetsHintFromExe()
{
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, buf, MAX_PATH))
        return "assets";
    std::string p(buf);
    auto slash = p.find_last_of("\\/");
    std::string dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
    return dir + "\\assets";
#else
    return "assets";
#endif
}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    // Visible console so user sees init stages (like a splash log)
    // AllocConsole if we were subsystem windows — we build as console subsystem
#endif

    Log("Starting Dogecoin Core Pro...");

    std::string assets = AssetsHintFromExe();
    std::string daemon = FindBestDogecoind(assets);
    if (daemon.empty()) {
        Log("ERROR: dogecoind.exe not found next to dogeinit.");
#if defined(_WIN32)
        MessageBoxA(nullptr,
                    "dogecoind.exe not found.\n\nPut dogecoind.exe next to dogeinit.exe.",
                    "dogeinit",
                    MB_OK | MB_ICONERROR);
#endif
        return 1;
    }

    std::string err;
    if (!IsDogecoindRunning()) {
        Log("Starting node...");
        if (!StartDogecoind(daemon, "", err)) {
            Log(("ERROR: " + err).c_str());
#if defined(_WIN32)
            MessageBoxA(nullptr, err.c_str(), "dogeinit", MB_OK | MB_ICONERROR);
#endif
            return 2;
        }
    }

    Log("Loading...");
    RpcConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 22555;
    RpcClient rpc(cfg);

    const int maxSec = 600; // 10 min for large datadirs
    bool ready = false;
    for (int i = 0; i < maxSec; ++i) {
        if (!IsDogecoindRunning() && i > 3) {
            Log("ERROR: node exited during startup. See debug.log.");
#if defined(_WIN32)
            MessageBoxA(nullptr,
                        "dogecoind exited during startup.\n\nSee %APPDATA%\\Dogecoin\\debug.log",
                        "dogeinit",
                        MB_OK | MB_ICONERROR);
#endif
            return 3;
        }

        if (rpc.portOpen(400)) {
            NodeSnapshot s = rpc.refreshSnapshot();
            if (s.connected) {
                ready = true;
                break;
            }
        }
        if (i > 0 && i % 15 == 0)
            Log(("Still loading... (" + std::to_string(i) + "s)").c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!ready) {
        Log("ERROR: timed out waiting for node");
#if defined(_WIN32)
        MessageBoxA(nullptr, "Timed out waiting for dogecoind (10 min).", "dogeinit", MB_OK | MB_ICONERROR);
#endif
        return 4;
    }

    std::string gui = FindProGuiExe();
    // Allow override: dogeinit.exe path\to\gui.exe
    if (argc >= 2)
        gui = argv[1];
    if (gui.empty()) {
        // Same folder smoke name
#if defined(_WIN32)
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string dir = buf;
        auto slash = dir.find_last_of("\\/");
        if (slash != std::string::npos)
            dir = dir.substr(0, slash + 1);
        gui = dir + "dogecoin-pro-gui-smoke.exe";
        if (!std::ifstream(gui))
            gui = dir + "dogecoin-pro-gui.exe";
#endif
    }

    if (!StartProGui(gui, err)) {
        Log(("ERROR: " + err).c_str());
#if defined(_WIN32)
        MessageBoxA(nullptr, ("Could not start GUI:\n" + gui + "\n" + err).c_str(), "dogeinit", MB_OK | MB_ICONERROR);
#endif
        return 5;
    }

    Log("Ready.");
    return 0;
}
