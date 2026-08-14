#include "arcade_host.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace ArcadeHost {

std::string CoreProUserAgent()
{
    // Full-looking UA + token GPE scans for (DogecoinCorePro/1.14.103)
    return std::string(
               "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
               "AppleWebKit/537.36 (KHTML, like Gecko) "
               "Chrome/120.0.0.0 Safari/537.36 ") +
           kUaToken;
}

#if defined(_WIN32)
static bool FileExistsW(const wchar_t* p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool LaunchProcess(const std::wstring& exe, const std::wstring& args)
{
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    STARTUPINFOW si {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi {};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    BOOL ok = CreateProcessW(exe.c_str(), buf.data(), nullptr, nullptr, FALSE,
                             0, nullptr, nullptr, &si, &pi);
    if (!ok)
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static bool TryEdgeOrChromeApp(const std::string& url)
{
    const wchar_t* candidates[] = {
        L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
    };

    // Build --app + user-agent (GPE unlock via UA token)
    std::string ua = CoreProUserAgent();
    std::wstring wurl(url.begin(), url.end());
    std::wstring wua(ua.begin(), ua.end());
    std::wstring args = L"--app=\"" + wurl + L"\" --user-agent=\"" + wua + L"\"";

    for (const wchar_t* exe : candidates) {
        if (!FileExistsW(exe))
            continue;
        if (LaunchProcess(exe, args))
            return true;
    }
    return false;
}
#endif

bool LaunchCabinetUnlocked(const std::string& url)
{
    std::string target = url.empty() ? kHubUrl : url;
#if defined(_WIN32)
    if (TryEdgeOrChromeApp(target))
        return true;
    // Fallback: default browser (no UA — games may stay LOCKED)
    HINSTANCE r = ShellExecuteA(nullptr, "open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
#else
    std::string cmd = "xdg-open \"" + target + "\" >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
#endif
}

} // namespace ArcadeHost
