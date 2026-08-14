// Copyright (c) 2026 The Dogecoin Core Pro developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_DBENGINE_H
#define DOGECOIN_DBENGINE_H

#include "fs.h"

#include <string>
#include <vector>

/**
 * Local KV engine for chainstate / block index (CDBWrapper).
 * This is not consensus and not on the wire. Other nodes never see it.
 * Default remains LevelDB until an MDBX (or other) backend is compiled in
 * and a datadir is created or migrated under an explicit flag.
 */
enum class DbEngine {
    NONE,     // empty directory / not created yet
    LEVELDB,
    MDBX,
    UNKNOWN,  // stamp or files we do not recognize
};

const char* DbEngineName(DbEngine e);
bool ParseDbEngine(const std::string& s, DbEngine& out);

/** -dbengine= (default leveldb). Unknown names -> UNKNOWN. */
DbEngine RequestedDbEngine();

/**
 * Pick the engine for this directory.
 * Explicit -dbengine= or engineOverride always wins (mismatch is refused later).
 * If the operator did not set -dbengine, honor an existing ENGINE stamp / files
 * so a migrated MDBX datadir opens without a flag. Empty dir -> LevelDB.
 */
DbEngine SelectDbEngine(const fs::path& path, DbEngine engineOverride = DbEngine::NONE);

/** Inspect an existing directory (ENGINE stamp, then LevelDB CURRENT, then MDBX files). */
DbEngine DetectExistingEngine(const fs::path& path);

/** Write path/ENGINE so a future binary will not open the wrong backend. */
bool WriteEngineStamp(const fs::path& path, DbEngine e);

/** Human message when -dbengine does not match files already on disk. */
std::string EngineMismatchMessage(DbEngine have, DbEngine want, const fs::path& path);

struct DbBatchOp {
    bool erase = false;
    std::string key;
    std::string value;
};

class CDbBackendIterator
{
public:
    virtual ~CDbBackendIterator() {}
    virtual bool Valid() const = 0;
    virtual void SeekToFirst() = 0;
    virtual void Seek(const std::string& key) = 0;
    virtual void Next() = 0;
    virtual std::string Key() const = 0;
    virtual std::string Value() const = 0;
};

/** Byte-oriented KV. Values stored here are already XOR-obfuscated by CDBWrapper. */
class CDbBackend
{
public:
    virtual ~CDbBackend() {}
    virtual bool Get(const std::string& key, std::string& value) const = 0;
    virtual bool Write(const std::vector<DbBatchOp>& ops, bool fSync) = 0;
    virtual CDbBackendIterator* NewIterator() = 0;
};

/**
 * Open the requested engine. LevelDB is default.
 * MDBX is compiled in; only allowed on an empty directory (or wipe).
 * Existing LevelDB datadirs are refused if -dbengine=mdbx.
 */
CDbBackend* CreateDbBackend(DbEngine engine,
                            const fs::path& path,
                            size_t nCacheSize,
                            bool fMemory,
                            bool fWipe,
                            std::string& errOut);

#endif // DOGECOIN_DBENGINE_H
