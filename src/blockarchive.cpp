// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockarchive.h"

#include "crypto/sha256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validation.h"

#include <boost/version.hpp>
#include <fstream>

static fs::path g_archivePath;

static fs::path AbsNorm(const fs::path& p)
{
    return fs::absolute(p);
}

static bool PathEqualsOrContains(const fs::path& a, const fs::path& b)
{
    const std::string as = AbsNorm(a).string();
    const std::string bs = AbsNorm(b).string();
    if (as == bs)
        return true;
    if (as.size() > bs.size() && as.compare(0, bs.size(), bs) == 0) {
        const char c = as[bs.size()];
        return c == '/' || c == '\\';
    }
    return false;
}

static bool CopyFileOverwrite(const fs::path& src, const fs::path& dest)
{
#if BOOST_VERSION >= 107400
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
#elif BOOST_VERSION >= 104000
    fs::copy_file(src, dest, fs::copy_option::overwrite_if_exists);
#else
    fs::copy_file(src, dest);
#endif
    return true;
}

static bool Sha256File(const fs::path& path, std::string& hexOut)
{
    FILE* f = fsbridge::fopen(path, "rb");
    if (!f)
        return false;
    CSHA256 hasher;
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        hasher.Write(buf, n);
    const bool ok = ferror(f) == 0;
    fclose(f);
    if (!ok)
        return false;
    unsigned char out[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(out);
    hexOut = HexStr(out, out + sizeof(out));
    return true;
}

static bool SidecarMatches(const fs::path& dest, const std::string& hex)
{
    const fs::path side = fs::path(dest.string() + ".sha256");
    if (!fs::exists(side))
        return false;
    std::ifstream in(side.string().c_str());
    std::string got;
    in >> got;
    return !got.empty() && got == hex;
}

static bool WriteSidecar(const fs::path& dest, const std::string& hex)
{
    const fs::path side = fs::path(dest.string() + ".sha256");
    std::ofstream out(side.string().c_str(), std::ios::trunc);
    if (!out)
        return false;
    out << hex << "\n";
    return (bool)out;
}

static bool ArchiveOne(const fs::path& src, const fs::path& destDir)
{
    if (!fs::exists(src))
        return true;
    const fs::path dest = destDir / src.filename();
    std::string hex;
    if (!Sha256File(src, hex)) {
        LogPrintf("archive: cannot hash %s\n", src.string());
        return false;
    }
    if (fs::exists(dest) && SidecarMatches(dest, hex)) {
        LogPrintf("archive: already have %s\n", dest.filename().string());
        return true;
    }
    try {
        CopyFileOverwrite(src, dest);
    } catch (const fs::filesystem_error& e) {
        LogPrintf("archive: copy %s -> %s failed: %s\n", src.string(), dest.string(), e.what());
        return false;
    }
    std::string destHex;
    if (!Sha256File(dest, destHex) || destHex != hex) {
        LogPrintf("archive: dest hash mismatch for %s\n", dest.string());
        return false;
    }
    if (!WriteSidecar(dest, hex))
        LogPrintf("archive: warning: could not write sidecar for %s\n", dest.string());
    LogPrintf("archive: copied %s (%s)\n", dest.filename().string(), hex.substr(0, 16));
    return true;
}

bool InitBlockArchive(std::string& err)
{
    err.clear();
    g_archivePath.clear();
    const std::string raw = GetArg("-archivepath", "");
    if (raw.empty())
        return true;

    g_archivePath = AbsNorm(fs::path(raw));
    const fs::path data = GetDataDir();
    const fs::path blocks = data / "blocks";
    const fs::path chainstate = data / "chainstate";
    if (PathEqualsOrContains(g_archivePath, data) &&
        (AbsNorm(g_archivePath) == AbsNorm(data) ||
         AbsNorm(g_archivePath) == AbsNorm(blocks) ||
         AbsNorm(g_archivePath) == AbsNorm(chainstate))) {
        err = "archivepath cannot be the live datadir, blocks/, or chainstate/";
        g_archivePath.clear();
        return false;
    }
    if (PathEqualsOrContains(g_archivePath, data)) {
        LogPrintf("Warning: -archivepath is inside the data directory. "
                  "That does not free local disk. Use a separate volume for a headless node.\n");
    }
    try {
        fs::create_directories(g_archivePath);
    } catch (const fs::filesystem_error& e) {
        err = strprintf("cannot create archivepath \"%s\": %s", g_archivePath.string(), e.what());
        g_archivePath.clear();
        return false;
    }
    LogPrintf("Block archive: copy-before-prune to %s (wallet/chainstate stay local)\n",
              g_archivePath.string());
    return true;
}

fs::path GetBlockArchivePath()
{
    return g_archivePath;
}

bool ArchiveBlockFilePair(int nFile)
{
    if (g_archivePath.empty())
        return true;
    CDiskBlockPos pos(nFile, 0);
    const fs::path blk = GetBlockPosFilename(pos, "blk");
    const fs::path rev = GetBlockPosFilename(pos, "rev");
    if (!fs::exists(blk)) {
        LogPrintf("archive: missing %s\n", blk.string());
        return false;
    }
    if (!ArchiveOne(blk, g_archivePath))
        return false;
    if (!ArchiveOne(rev, g_archivePath))
        return false;
    return true;
}

BlockArchiveInfo GetBlockArchiveInfo()
{
    BlockArchiveInfo info;
    if (g_archivePath.empty())
        return info;
    info.enabled = true;
    info.path = g_archivePath.string();
    try {
        for (fs::directory_iterator it(g_archivePath); it != fs::directory_iterator(); ++it) {
            const fs::path p = it->path();
            const std::string name = p.filename().string();
            if (name.size() > 7 && name.compare(name.size() - 7, 7, ".sha256") == 0) {
                info.sidecars++;
                continue;
            }
            if (!fs::is_regular_file(p))
                continue;
            info.files++;
            boost::system::error_code ec;
            const uintmax_t sz = fs::file_size(p, ec);
            if (!ec)
                info.bytes += (uint64_t)sz;
        }
    } catch (const fs::filesystem_error& e) {
        LogPrintf("archive: list failed: %s\n", e.what());
    }
    return info;
}

BlockArchiveVerify VerifyBlockArchive(int maxFiles)
{
    BlockArchiveVerify r;
    if (g_archivePath.empty() || !fs::exists(g_archivePath))
        return r;
    try {
        for (fs::directory_iterator it(g_archivePath); it != fs::directory_iterator(); ++it) {
            const fs::path p = it->path();
            const std::string name = p.filename().string();
            if (name.size() > 7 && name.compare(name.size() - 7, 7, ".sha256") == 0)
                continue;
            if (!fs::is_regular_file(p))
                continue;
            if (maxFiles > 0 && r.checked >= maxFiles)
                break;
            r.checked++;
            const fs::path side = fs::path(p.string() + ".sha256");
            if (!fs::exists(side)) {
                r.missing_sidecar++;
                continue;
            }
            std::ifstream in(side.string().c_str());
            std::string expect;
            in >> expect;
            std::string got;
            if (expect.empty() || !Sha256File(p, got) || got != expect) {
                r.failed++;
                if (r.failed_names.size() < 20)
                    r.failed_names.push_back(name);
            } else {
                r.ok++;
            }
        }
    } catch (const fs::filesystem_error& e) {
        LogPrintf("archive: verify failed: %s\n", e.what());
    }
    return r;
}
