// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_CLOUDPATH_H
#define DOGECOIN_CLOUDPATH_H

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <limits.h>
#include <stdlib.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

/**
 * Live datadir must stay on a local disk.
 *
 * Consumer / desktop sync agents (OneDrive, Drive, Dropbox, iCloud, Proton,
 * pCloud, MEGA, Nextcloud, …) lock and rewrite files. That crashes or
 * corrupts a running node. A later archive folder of finalized, read-only
 * blk*.dat files may live on these services; this check is only for the
 * hot datadir (wallet, chainstate, active blocks).
 *
 * Windows: OneDrive env + Desktop clients.
 * macOS:   ~/Library/CloudStorage/* and iCloud Mobile Documents.
 * Linux:   official clients + common FUSE mounts (pCloud, MEGA, Nextcloud).
 */
inline std::string NormalizeForCloudPathCheck(const std::string& in)
{
    std::string s = in;
#if defined(_WIN32)
    char full[MAX_PATH];
    DWORD n = GetFullPathNameA(in.c_str(), MAX_PATH, full, nullptr);
    if (n > 0 && n < MAX_PATH)
        s = full;
#else
    char resolved[PATH_MAX];
    if (!in.empty() && realpath(in.c_str(), resolved) != nullptr)
        s = resolved;
#endif
    for (char& c : s) {
        if (c == '/')
            c = '\\';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (!s.empty() && s.back() == '\\')
        s.pop_back();
    return s;
}

/** True if `name` is a path component, or `name - …` (OneDrive business). */
inline bool CloudPathHasFolder(const std::string& norm, const char* name)
{
    if (!name || !name[0])
        return false;
    const std::string key = std::string("\\") + name;
    size_t p = 0;
    while ((p = norm.find(key, p)) != std::string::npos) {
        const size_t after = p + key.size();
        if (after == norm.size() || norm[after] == '\\' ||
            norm.compare(after, 3, " - ") == 0)
            return true;
        p = after;
    }
    return false;
}

inline bool LooksLikeConsumerCloudPath(const std::string& path)
{
    if (path.empty())
        return false;
    const std::string s = NormalizeForCloudPathCheck(path);
    if (s.empty())
        return false;

    // Multi-segment locations (macOS File Provider + Google Drive FS).
    static const char* substrings[] = {
        "\\library\\cloudstorage",          // macOS ~/Library/CloudStorage/*
        "\\mobile documents",               // iCloud Drive
        "\\google\\drive",                  // Google Drive for desktop
        "\\com~apple~clouddocs",
    };
    for (const char* sub : substrings) {
        if (s.find(sub) != std::string::npos)
            return true;
    }

    // Folder names used by the actual desktop / FUSE clients.
    static const char* folders[] = {
        // Cross-platform "big four"
        "onedrive",
        "dropbox",
        "google drive",
        "googledrive",
        "my drive",
        "icloud",
        "icloud drive",
        "iclouddrive",
        "proton drive",
        "protondrive",
        "proton-drive",
        // Linux-strong + extra desktop clients
        "pcloud",
        "pclouddrive",
        "pcloud drive",
        "mega",
        "megasync",
        "nextcloud",
        "owncloud",
        "box sync",
        "box",
        "insync",
        "tresorit",
        "icedrive",
        "filen",
        "sync.com",
        "syncthing",
        "rclone",
        "odrive",
    };
    for (const char* name : folders) {
        if (CloudPathHasFolder(s, name))
            return true;
    }

    static const char* envVars[] = {
        "OneDrive",
        "OneDriveConsumer",
        "OneDriveCommercial",
        "DROPBOX_HOME",
        "PCLOUD_DRIVE_PATH",
    };
    for (const char* name : envVars) {
        const char* v = std::getenv(name);
        if (!v || !v[0])
            continue;
        std::string ev = NormalizeForCloudPathCheck(v);
        if (ev.empty())
            continue;
        if (s == ev || (s.size() > ev.size() && s.compare(0, ev.size(), ev) == 0 &&
                        s[ev.size()] == '\\'))
            return true;
    }
    return false;
}

inline bool LooksLikeConsumerCloudPath(const char* path)
{
    return path && path[0] && LooksLikeConsumerCloudPath(std::string(path));
}

#if !defined(_WIN32)
#include <cstdio>
/** Linux: fstype of the mount that covers `path` (empty if unknown). */
inline std::string LinuxMountFstype(const std::string& path)
{
    FILE* f = std::fopen("/proc/mounts", "r");
    if (!f)
        return {};
    std::string bestMp, bestType;
    char dev[256], mp[512], type[64];
    while (std::fscanf(f, "%255s %511s %63s %*s %*d %*d\n", dev, mp, type) == 3) {
        std::string mount = mp;
        if (path.compare(0, mount.size(), mount) == 0 &&
            (path.size() == mount.size() || path[mount.size()] == '/') &&
            mount.size() >= bestMp.size()) {
            bestMp = mount;
            bestType = type;
        }
    }
    std::fclose(f);
    return bestType;
}
#endif

/**
 * Operator / server file stores: Vultr File System (virtiofs), NFS, CIFS, Ceph.
 * Allowed as an archive or snapshot destination. Warn if used as live datadir.
 */
inline bool LooksLikeOperatorFileStore(const std::string& path)
{
    if (path.empty())
        return false;
    const std::string s = NormalizeForCloudPathCheck(path);
    static const char* folders[] = {
        "vfs",              // /mnt/vfs  (Vultr File System default)
        "gpenodestore",
        "virtiofs",
        "nfs",
        "cifs",
        "smb",
    };
    for (const char* name : folders) {
        if (CloudPathHasFolder(s, name))
            return true;
    }
    if (s.find("\\mnt\\vfs") != std::string::npos)
        return true;
#if !defined(_WIN32)
    const std::string fstype = LinuxMountFstype(path);
    if (fstype == "virtiofs" || fstype == "nfs" || fstype == "nfs4" ||
        fstype == "cifs" || fstype == "smb3" || fstype == "9p" ||
        fstype == "ceph" || fstype == "fuse.ceph" || fstype == "glusterfs")
        return true;
#endif
    return false;
}

inline bool LooksLikeOperatorFileStore(const char* path)
{
    return path && path[0] && LooksLikeOperatorFileStore(std::string(path));
}

#endif // DOGECOIN_CLOUDPATH_H
