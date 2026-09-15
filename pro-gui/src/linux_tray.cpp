#if !defined(_WIN32)
#include "linux_tray.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t g_show = 0;
static volatile sig_atomic_t g_quit = 0;
static std::string g_pidPath;

static std::string PidPath()
{
    if (const char* rd = std::getenv("XDG_RUNTIME_DIR")) {
        if (rd[0])
            return std::string(rd) + "/dogecoin-pro-gui.pid";
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "/tmp/dogecoin-pro-gui-%d.pid", (int)getuid());
    return buf;
}

static void OnUsr1(int)
{
    g_show = 1;
}

static void OnUsr2(int)
{
    g_quit = 1;
}

void LinuxTrayInit()
{
    g_pidPath = PidPath();
    struct sigaction sa {};
    sa.sa_handler = OnUsr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, nullptr);
    sa.sa_handler = OnUsr2;
    sigaction(SIGUSR2, &sa, nullptr);
    LinuxTrayWritePid();
}

void LinuxTrayWritePid()
{
    if (g_pidPath.empty())
        g_pidPath = PidPath();
    FILE* f = std::fopen(g_pidPath.c_str(), "w");
    if (!f)
        return;
    std::fprintf(f, "%d\n", (int)getpid());
    std::fclose(f);
}

void LinuxTrayClearPid()
{
    if (g_pidPath.empty())
        g_pidPath = PidPath();
    unlink(g_pidPath.c_str());
}

void LinuxTrayEnsureHelper(const char* installDir)
{
    if (system("pgrep -x gpenode-tray >/dev/null 2>&1") == 0)
        return;
    std::string cmd;
    if (installDir && installDir[0]) {
        cmd = "\"";
        cmd += installDir;
        cmd += "/gpenode-tray\"";
        if (access((std::string(installDir) + "/gpenode-tray").c_str(), X_OK) != 0)
            cmd = "gpenode-tray";
    } else {
        cmd = "gpenode-tray";
    }
    cmd += " >/dev/null 2>&1 &";
    (void)system(cmd.c_str());
}

void LinuxTrayNotify(const char* title, const char* body)
{
    if (system("command -v notify-send >/dev/null 2>&1") != 0)
        return;
    std::string cmd = "notify-send --app-name='Dogecoin Core Pro' ";
    if (title && title[0]) {
        cmd += "'";
        cmd += title;
        cmd += "' ";
    }
    cmd += "'";
    cmd += (body && body[0]) ? body : "Still running in the system tray.";
    cmd += "' >/dev/null 2>&1 &";
    (void)system(cmd.c_str());
}

bool LinuxTrayPollShow()
{
    if (!g_show)
        return false;
    g_show = 0;
    return true;
}

bool LinuxTrayPollQuit()
{
    if (!g_quit)
        return false;
    g_quit = 0;
    return true;
}

#endif
