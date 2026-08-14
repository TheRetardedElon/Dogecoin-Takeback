// Copyright (c) 2026 The Dogecoin Core Pro developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbmigrate.h"

#include "dbwrapper.h"
#include "ui_interface.h"
#include "util.h"

#include <memory>

static const size_t MIGRATE_BATCH = 4096;
static const size_t MIGRATE_CACHE = 64 << 20;

bool CopyKvDirectory(const fs::path& src,
                     DbEngine srcEngine,
                     const fs::path& dst,
                     DbEngine dstEngine,
                     uint64_t& keysOut,
                     std::string& errOut)
{
    keysOut = 0;
    errOut.clear();

    if (srcEngine == DbEngine::NONE)
        srcEngine = DetectExistingEngine(src);
    if (srcEngine == DbEngine::NONE || srcEngine == DbEngine::UNKNOWN) {
        errOut = strprintf("Cannot detect source engine at %s", src.string());
        return false;
    }
    if (dstEngine == DbEngine::NONE || dstEngine == DbEngine::UNKNOWN) {
        errOut = "Destination engine must be leveldb or mdbx";
        return false;
    }
    if (srcEngine == dstEngine) {
        errOut = "Source and destination engines are the same";
        return false;
    }

    const DbEngine destHave = DetectExistingEngine(dst);
    if (destHave != DbEngine::NONE) {
        errOut = strprintf("Destination %s already has a database (%s). Refusing to overwrite.",
                           dst.string(), DbEngineName(destHave));
        return false;
    }

    try {
        CDBWrapper srcDb(src, MIGRATE_CACHE, false, false, false, srcEngine);
        CDBWrapper dstDb(dst, MIGRATE_CACHE, false, false, false, dstEngine);

        std::unique_ptr<CDBIterator> it(srcDb.NewIterator());
        CDBBatch batch(dstDb);
        size_t pending = 0;

        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            batch.WriteRaw(it->GetRawKey(), it->GetRawValue());
            pending++;
            keysOut++;
            if (pending >= MIGRATE_BATCH) {
                if (!dstDb.WriteBatch(batch, false)) {
                    errOut = "WriteBatch failed during migration";
                    return false;
                }
                batch.Clear();
                pending = 0;
                if ((keysOut % 100000) == 0) {
                    uiInterface.InitMessage(strprintf("Migrating… %llu keys", (unsigned long long)keysOut));
                    LogPrintf("Migrate %s -> %s: %llu keys\n", src.string(), dst.string(),
                              (unsigned long long)keysOut);
                }
            }
        }
        if (pending > 0 && !dstDb.WriteBatch(batch, true)) {
            errOut = "Final WriteBatch failed during migration";
            return false;
        }
        dstDb.Sync();
    } catch (const std::exception& e) {
        errOut = e.what();
        return false;
    }

    LogPrintf("Copied %llu keys %s (%s) -> %s (%s) (raw bytes; source left intact)\n",
              (unsigned long long)keysOut, src.string(), DbEngineName(srcEngine),
              dst.string(), DbEngineName(dstEngine));
    return true;
}

bool RunRequestedDbMigration(const fs::path& datadir, std::string& errOut)
{
    errOut.clear();
    if (!IsArgSet("-migratedb"))
        return true;

    DbEngine dest = DbEngine::NONE;
    if (!ParseDbEngine(GetArg("-migratedb", ""), dest) || dest == DbEngine::UNKNOWN || dest == DbEngine::NONE) {
        errOut = "Invalid -migratedb (use mdbx or leveldb)";
        return false;
    }

    const fs::path chainSrc = datadir / "chainstate";
    const fs::path indexSrc = datadir / "blocks" / "index";
    const std::string tag = DbEngineName(dest);
    const fs::path chainDst = datadir / (std::string("chainstate_") + tag);
    const fs::path indexDst = datadir / "blocks" / (std::string("index_") + tag);

    uint64_t nChain = 0, nIndex = 0;
    uiInterface.InitMessage(_("Copying chainstate (local KV only)…"));
    if (DetectExistingEngine(chainSrc) != DbEngine::NONE) {
        if (!CopyKvDirectory(chainSrc, DbEngine::NONE, chainDst, dest, nChain, errOut))
            return false;
    } else {
        LogPrintf("No chainstate at %s — skip\n", chainSrc.string());
    }

    uiInterface.InitMessage(_("Copying block index (local KV only)…"));
    if (DetectExistingEngine(indexSrc) != DbEngine::NONE) {
        if (!CopyKvDirectory(indexSrc, DbEngine::NONE, indexDst, dest, nIndex, errOut))
            return false;
    } else {
        LogPrintf("No block index at %s — skip\n", indexSrc.string());
    }

    if (GetBoolArg("-swapdb", false)) {
        auto swapOne = [&](const fs::path& live, const fs::path& incoming) -> bool {
            if (!fs::exists(incoming))
                return true;
            const std::string bakName = live.filename().string() + std::string("_") +
                                        DbEngineName(DetectExistingEngine(live) == DbEngine::NONE
                                                         ? DbEngine::LEVELDB
                                                         : DetectExistingEngine(live));
            fs::path bak = live.parent_path() / bakName;
            if (fs::exists(bak)) {
                errOut = strprintf("Backup %s already exists — not swapping.", bak.string());
                return false;
            }
            try {
                if (fs::exists(live))
                    fs::rename(live, bak);
                fs::rename(incoming, live);
            } catch (const fs::filesystem_error& e) {
                errOut = e.what();
                return false;
            }
            LogPrintf("Swapped %s -> %s, %s -> %s\n",
                      live.string(), bak.string(), incoming.string(), live.string());
            return true;
        };
        if (!swapOne(chainSrc, chainDst) || !swapOne(indexSrc, indexDst))
            return false;
        LogPrintf("KV swap complete. Next open uses ENGINE stamp (default still LevelDB for new dirs).\n");
        return true;
    }

    errOut = strprintf(
        "Local KV copy finished (consensus unchanged). "
        "Wrote %llu chainstate keys to %s and %llu index keys to %s. "
        "Original folders were not deleted. "
        "To switch: restart with -migratedb=%s -swapdb (renames live folders aside), "
        "or rename by hand then start — the ENGINE stamp is enough, -dbengine is optional. "
        "To stay on LevelDB, do nothing.",
        (unsigned long long)nChain, chainDst.string(),
        (unsigned long long)nIndex, indexDst.string(),
        tag);
    // Returning false with this message stops startup on purpose (fail closed after copy).
    return false;
}
