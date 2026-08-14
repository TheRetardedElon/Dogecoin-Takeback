// Copyright (c) 2026 The Dogecoin Core Pro developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbengine.h"

#include "fs.h"
#include "util.h"

#include <cctype>
#include <fstream>

static std::string Lower(std::string s)
{
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

const char* DbEngineName(DbEngine e)
{
    switch (e) {
    case DbEngine::LEVELDB:
        return "leveldb";
    case DbEngine::MDBX:
        return "mdbx";
    case DbEngine::NONE:
        return "none";
    case DbEngine::UNKNOWN:
    default:
        return "unknown";
    }
}

bool ParseDbEngine(const std::string& s, DbEngine& out)
{
    const std::string v = Lower(s);
    if (v.empty() || v == "leveldb" || v == "ldb") {
        out = DbEngine::LEVELDB;
        return true;
    }
    if (v == "mdbx" || v == "libmdbx") {
        out = DbEngine::MDBX;
        return true;
    }
    out = DbEngine::UNKNOWN;
    return false;
}

DbEngine RequestedDbEngine()
{
    DbEngine e = DbEngine::LEVELDB;
    ParseDbEngine(GetArg("-dbengine", "leveldb"), e);
    return e;
}

DbEngine SelectDbEngine(const fs::path& path, DbEngine engineOverride)
{
    if (engineOverride != DbEngine::NONE)
        return engineOverride;
    const DbEngine have = DetectExistingEngine(path);
    // Live files win. A stale dogecoin.conf `dbengine=leveldb` must not
    // abort an MDBX IBD (Error opening block database → instant exit).
    if (have == DbEngine::LEVELDB || have == DbEngine::MDBX) {
        if (IsArgSet("-dbengine")) {
            const DbEngine want = RequestedDbEngine();
            if (want != have)
                LogPrintf("Ignoring -dbengine=%s because %s is already %s\n",
                          DbEngineName(want), path.string(), DbEngineName(have));
        }
        return have;
    }
    if (IsArgSet("-dbengine"))
        return RequestedDbEngine();
    return DbEngine::LEVELDB;
}

static fs::path StampPath(const fs::path& dir)
{
    return dir / "ENGINE";
}

static bool HasLevelDbFiles(const fs::path& path)
{
    return fs::exists(path / "CURRENT");
}

static bool HasMdbxFiles(const fs::path& path)
{
    return fs::exists(path / "mdbx.dat") || fs::exists(path / "data.mdb");
}

DbEngine DetectExistingEngine(const fs::path& path)
{
    if (path.empty() || !fs::exists(path) || !fs::is_directory(path))
        return DbEngine::NONE;

    const fs::path stamp = StampPath(path);
    if (fs::exists(stamp)) {
        std::ifstream f(stamp.string().c_str());
        std::string line;
        if (f && std::getline(f, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                line.pop_back();
            DbEngine e;
            if (ParseDbEngine(line, e) && e != DbEngine::UNKNOWN) {
                // Stale ENGINE after a wipe (no real DB files) must not block a fresh start.
                if (e == DbEngine::LEVELDB && !HasLevelDbFiles(path))
                    return DbEngine::NONE;
                if (e == DbEngine::MDBX && !HasMdbxFiles(path))
                    return DbEngine::NONE;
                return e;
            }
            return DbEngine::UNKNOWN;
        }
    }

    // Legacy LevelDB (no stamp): CURRENT + MANIFEST / LOCK
    if (HasLevelDbFiles(path))
        return DbEngine::LEVELDB;

    // libmdbx default files
    if (HasMdbxFiles(path))
        return DbEngine::MDBX;

    return DbEngine::NONE;
}

bool WriteEngineStamp(const fs::path& path, DbEngine e)
{
    if (path.empty() || e == DbEngine::NONE || e == DbEngine::UNKNOWN)
        return false;
    try {
        fs::create_directories(path);
    } catch (const fs::filesystem_error&) {
        return false;
    }
    std::ofstream f(StampPath(path).string().c_str(), std::ios::trunc);
    if (!f)
        return false;
    f << DbEngineName(e) << "\n";
    return (bool)f;
}

std::string EngineMismatchMessage(DbEngine have, DbEngine want, const fs::path& path)
{
    return strprintf(
        "Datadir KV engine mismatch at %s: on disk=%s, requested=%s. "
        "Local storage is not consensus — do not convert in place. "
        "Use a new datadir, or Fast Sync into a fresh directory, or reopen with -dbengine=%s.",
        path.string(), DbEngineName(have), DbEngineName(want), DbEngineName(have));
}
