// Copyright (c) 2012-2016 The Bitcoin Core developers
// Copyright (c) 2026 The Dogecoin Core Pro developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbwrapper.h"

#include "fs.h"
#include "random.h"
#include "util.h"

#include <leveldb/cache.h>
#include <leveldb/db.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <leveldb/write_batch.h>
#include <memenv.h>

#include "mdbx/mdbx.h"

#include <algorithm>
#include <stdint.h>

static void HandleLevelDbError(const leveldb::Status& status)
{
    if (status.ok())
        return;
    LogPrintf("%s\n", status.ToString());
    if (status.IsCorruption())
        throw dbwrapper_error("Database corrupted");
    if (status.IsIOError())
        throw dbwrapper_error("Database I/O error");
    if (status.IsNotFound())
        throw dbwrapper_error("Database entry missing");
    throw dbwrapper_error("Unknown database error");
}

static leveldb::Options MakeLevelDbOptions(size_t nCacheSize)
{
    leveldb::Options options;
    options.block_cache = leveldb::NewLRUCache(nCacheSize / 2);
    options.write_buffer_size = nCacheSize / 4;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.compression = leveldb::kNoCompression;
    options.max_open_files = 64;
    if (leveldb::kMajorVersion > 1 || (leveldb::kMajorVersion == 1 && leveldb::kMinorVersion >= 16)) {
        options.paranoid_checks = true;
    }
    return options;
}

class LevelDbIterator : public CDbBackendIterator
{
    leveldb::Iterator* piter;
public:
    explicit LevelDbIterator(leveldb::Iterator* it) : piter(it) {}
    ~LevelDbIterator() { delete piter; }
    bool Valid() const { return piter && piter->Valid(); }
    void SeekToFirst() { piter->SeekToFirst(); }
    void Seek(const std::string& key)
    {
        piter->Seek(leveldb::Slice(key.data(), key.size()));
    }
    void Next() { piter->Next(); }
    std::string Key() const
    {
        leveldb::Slice s = piter->key();
        return std::string(s.data(), s.size());
    }
    std::string Value() const
    {
        leveldb::Slice s = piter->value();
        return std::string(s.data(), s.size());
    }
};

class LevelDbBackend : public CDbBackend
{
    leveldb::Env* penv;
    leveldb::Options options;
    leveldb::ReadOptions readoptions;
    leveldb::ReadOptions iteroptions;
    leveldb::WriteOptions writeoptions;
    leveldb::WriteOptions syncoptions;
    leveldb::DB* pdb;

public:
    LevelDbBackend(const fs::path& path, size_t nCacheSize, bool fMemory, bool fWipe)
        : penv(NULL), pdb(NULL)
    {
        readoptions.verify_checksums = true;
        iteroptions.verify_checksums = true;
        iteroptions.fill_cache = false;
        syncoptions.sync = true;
        options = MakeLevelDbOptions(nCacheSize);
        options.create_if_missing = true;
        if (fMemory) {
            penv = leveldb::NewMemEnv(leveldb::Env::Default());
            options.env = penv;
        } else {
            if (fWipe) {
                LogPrintf("Wiping LevelDB in %s\n", path.string());
                leveldb::Status result = leveldb::DestroyDB(path.string(), options);
                HandleLevelDbError(result);
            }
            TryCreateDirectory(path);
            LogPrintf("Opening LevelDB in %s\n", path.string());
        }
        leveldb::Status status = leveldb::DB::Open(options, path.string(), &pdb);
        HandleLevelDbError(status);
        LogPrintf("Opened LevelDB successfully\n");
    }

    ~LevelDbBackend()
    {
        delete pdb;
        pdb = NULL;
        delete options.filter_policy;
        options.filter_policy = NULL;
        delete options.block_cache;
        options.block_cache = NULL;
        delete penv;
        options.env = NULL;
    }

    bool Get(const std::string& key, std::string& value) const
    {
        leveldb::Status status = pdb->Get(readoptions, leveldb::Slice(key.data(), key.size()), &value);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            HandleLevelDbError(status);
        }
        return true;
    }

    bool Write(const std::vector<DbBatchOp>& ops, bool fSync)
    {
        leveldb::WriteBatch batch;
        for (size_t i = 0; i < ops.size(); ++i) {
            const DbBatchOp& op = ops[i];
            leveldb::Slice slKey(op.key.data(), op.key.size());
            if (op.erase)
                batch.Delete(slKey);
            else
                batch.Put(slKey, leveldb::Slice(op.value.data(), op.value.size()));
        }
        leveldb::Status status = pdb->Write(fSync ? syncoptions : writeoptions, &batch);
        HandleLevelDbError(status);
        return true;
    }

    CDbBackendIterator* NewIterator()
    {
        return new LevelDbIterator(pdb->NewIterator(iteroptions));
    }
};

static void HandleMdbxError(int rc, const char* what)
{
    // MDBX_RESULT_TRUE (-1) is success: e.g. env_sync had nothing pending.
    if (rc == MDBX_SUCCESS || rc == MDBX_RESULT_TRUE)
        return;
    const char* msg = mdbx_strerror(rc);
    if (!msg || !msg[0])
        msg = "mdbx error";
    LogPrintf("MDBX %s: %s (%d)\n", what, msg, rc);
    if (rc == MDBX_NOTFOUND)
        throw dbwrapper_error("Database entry missing");
    throw dbwrapper_error(strprintf("MDBX %s: %s (%d)", what, msg, rc));
}

class MdbxIterator : public CDbBackendIterator
{
    MDBX_txn* txn;
    MDBX_cursor* cur;
    bool valid;
    std::string keyStore;
    std::string valStore;

    void LoadCurrent()
    {
        MDBX_val k, v;
        int rc = mdbx_cursor_get(cur, &k, &v, MDBX_GET_CURRENT);
        if (rc == MDBX_SUCCESS) {
            keyStore.assign((const char*)k.iov_base, k.iov_len);
            valStore.assign((const char*)v.iov_base, v.iov_len);
            valid = true;
        } else {
            valid = false;
            keyStore.clear();
            valStore.clear();
        }
    }

public:
    MdbxIterator(MDBX_env* env, MDBX_dbi dbi) : txn(NULL), cur(NULL), valid(false)
    {
        int rc = mdbx_txn_begin(env, NULL, MDBX_TXN_RDONLY, &txn);
        HandleMdbxError(rc, "txn_begin(read)");
        rc = mdbx_cursor_open(txn, dbi, &cur);
        if (rc != MDBX_SUCCESS) {
            mdbx_txn_abort(txn);
            txn = NULL;
            HandleMdbxError(rc, "cursor_open");
        }
    }
    ~MdbxIterator()
    {
        if (cur)
            mdbx_cursor_close(cur);
        if (txn)
            mdbx_txn_abort(txn);
    }
    bool Valid() const { return valid; }
    void SeekToFirst()
    {
        MDBX_val k, v;
        int rc = mdbx_cursor_get(cur, &k, &v, MDBX_FIRST);
        if (rc == MDBX_SUCCESS) {
            keyStore.assign((const char*)k.iov_base, k.iov_len);
            valStore.assign((const char*)v.iov_base, v.iov_len);
            valid = true;
        } else {
            valid = false;
        }
    }
    void Seek(const std::string& key)
    {
        MDBX_val k, v;
        k.iov_base = (void*)key.data();
        k.iov_len = key.size();
        int rc = mdbx_cursor_get(cur, &k, &v, MDBX_SET_RANGE);
        if (rc == MDBX_SUCCESS) {
            keyStore.assign((const char*)k.iov_base, k.iov_len);
            valStore.assign((const char*)v.iov_base, v.iov_len);
            valid = true;
        } else {
            valid = false;
        }
    }
    void Next()
    {
        if (!valid)
            return;
        MDBX_val k, v;
        int rc = mdbx_cursor_get(cur, &k, &v, MDBX_NEXT);
        if (rc == MDBX_SUCCESS) {
            keyStore.assign((const char*)k.iov_base, k.iov_len);
            valStore.assign((const char*)v.iov_base, v.iov_len);
            valid = true;
        } else {
            valid = false;
        }
    }
    std::string Key() const { return keyStore; }
    std::string Value() const { return valStore; }
};

class MdbxBackend : public CDbBackend
{
    MDBX_env* env;
    MDBX_dbi dbi;

    static void WipeMdbxFiles(const fs::path& path)
    {
        const char* names[] = {"mdbx.dat", "mdbx.lck", "data.mdb", "lock.mdb", "ENGINE", NULL};
        for (int i = 0; names[i]; ++i) {
            try {
                fs::remove(path / names[i]);
            } catch (...) {
            }
        }
    }

public:
    MdbxBackend(const fs::path& path, size_t nCacheSize, bool fWipe) : env(NULL), dbi(0)
    {
        if (fWipe)
            WipeMdbxFiles(path);
        TryCreateDirectory(path);
        LogPrintf("Opening MDBX in %s\n", path.string());

        int rc = mdbx_env_create(&env);
        HandleMdbxError(rc, "env_create");

        // size_now must be >= the existing mdbx.dat. After IBD the file is
        // hundreds of MB; passing 64 MiB here makes env_open fail and the
        // process exits (looks like "something is killing dogecoind").
        const intptr_t step = (intptr_t)(64u << 20);
        intptr_t size_now = (intptr_t)std::max((size_t)(64u << 20), nCacheSize * 8);
        try {
            const fs::path dat = path / "mdbx.dat";
            if (fs::exists(dat)) {
                const uintmax_t have = fs::file_size(dat);
                if (have > (uintmax_t)size_now)
                    size_now = (intptr_t)have;
            }
        } catch (...) {
        }
        size_now = ((size_now + step - 1) / step) * step;
        const intptr_t size_upper = (intptr_t)((uint64_t)1 << 40); // 1 TiB cap; grows as needed
        LogPrintf("MDBX geometry size_now=%d MiB upper=1 TiB in %s\n",
                  (int)(size_now / (1024 * 1024)), path.string());
        rc = mdbx_env_set_geometry(env, 1 << 20, size_now, size_upper, step, -1, -1);
        HandleMdbxError(rc, "env_set_geometry");

        rc = mdbx_env_open(env, path.string().c_str(), (MDBX_env_flags_t)0, 0644);
        HandleMdbxError(rc, "env_open");

        MDBX_txn* txn = NULL;
        rc = mdbx_txn_begin(env, NULL, (MDBX_txn_flags_t)0, &txn);
        HandleMdbxError(rc, "txn_begin(open dbi)");
        rc = mdbx_dbi_open(txn, NULL, MDBX_CREATE, &dbi);
        if (rc != MDBX_SUCCESS) {
            mdbx_txn_abort(txn);
            HandleMdbxError(rc, "dbi_open");
        }
        rc = mdbx_txn_commit(txn);
        HandleMdbxError(rc, "txn_commit(open dbi)");
        LogPrintf("Opened MDBX successfully\n");
    }

    ~MdbxBackend()
    {
        if (env)
            mdbx_env_close(env);
        env = NULL;
    }

    bool Get(const std::string& key, std::string& value) const
    {
        MDBX_txn* txn = NULL;
        int rc = mdbx_txn_begin(env, NULL, MDBX_TXN_RDONLY, &txn);
        HandleMdbxError(rc, "txn_begin(get)");
        MDBX_val k, v;
        k.iov_base = (void*)key.data();
        k.iov_len = key.size();
        rc = mdbx_get(txn, dbi, &k, &v);
        if (rc == MDBX_NOTFOUND) {
            mdbx_txn_abort(txn);
            return false;
        }
        if (rc != MDBX_SUCCESS) {
            mdbx_txn_abort(txn);
            HandleMdbxError(rc, "get");
        }
        value.assign((const char*)v.iov_base, v.iov_len);
        mdbx_txn_abort(txn);
        return true;
    }

    bool Write(const std::vector<DbBatchOp>& ops, bool fSync)
    {
        MDBX_txn* txn = NULL;
        int rc = mdbx_txn_begin(env, NULL, (MDBX_txn_flags_t)0, &txn);
        HandleMdbxError(rc, "txn_begin(write)");
        for (size_t i = 0; i < ops.size(); ++i) {
            const DbBatchOp& op = ops[i];
            MDBX_val k, v;
            k.iov_base = (void*)op.key.data();
            k.iov_len = op.key.size();
            if (op.erase) {
                rc = mdbx_del(txn, dbi, &k, NULL);
                if (rc != MDBX_SUCCESS && rc != MDBX_NOTFOUND) {
                    mdbx_txn_abort(txn);
                    HandleMdbxError(rc, "del");
                }
            } else {
                v.iov_base = (void*)op.value.data();
                v.iov_len = op.value.size();
                rc = mdbx_put(txn, dbi, &k, &v, (MDBX_put_flags_t)0);
                if (rc != MDBX_SUCCESS) {
                    mdbx_txn_abort(txn);
                    HandleMdbxError(rc, "put");
                }
            }
        }
        rc = mdbx_txn_commit(txn);
        HandleMdbxError(rc, "txn_commit(write)");
        if (fSync) {
            rc = mdbx_env_sync_ex(env, true, false);
            HandleMdbxError(rc, "env_sync");
        }
        return true;
    }

    CDbBackendIterator* NewIterator()
    {
        return new MdbxIterator(env, dbi);
    }
};

CDbBackend* CreateDbBackend(DbEngine engine,
                            const fs::path& path,
                            size_t nCacheSize,
                            bool fMemory,
                            bool fWipe,
                            std::string& errOut)
{
    errOut.clear();
    if (engine == DbEngine::LEVELDB)
        return new LevelDbBackend(path, nCacheSize, fMemory, fWipe);
    if (engine == DbEngine::MDBX) {
        try {
            return new MdbxBackend(path, nCacheSize, fWipe);
        } catch (const std::exception& e) {
            errOut = e.what();
            return NULL;
        }
    }
    errOut = strprintf("Unknown or unsupported -dbengine (%s)", DbEngineName(engine));
    return NULL;
}

CDBWrapper::CDBWrapper(const fs::path& path, size_t nCacheSize, bool fMemory, bool fWipe, bool obfuscate, DbEngine engineOverride)
{
    engine = SelectDbEngine(path, engineOverride);
    if (engine == DbEngine::UNKNOWN) {
        throw dbwrapper_error("Invalid -dbengine (supported: leveldb, mdbx)");
    }

    if (!fMemory) {
        const DbEngine have = DetectExistingEngine(path);
        if (have != DbEngine::NONE && have != engine) {
            throw dbwrapper_error(EngineMismatchMessage(have, engine, path));
        }
    }

    std::string err;
    CDbBackend* raw = CreateDbBackend(engine, path, nCacheSize, fMemory, fWipe, err);
    if (!raw)
        throw dbwrapper_error(err.empty() ? "Failed to open database backend" : err);
    backend.reset(raw);

    if (!fMemory)
        WriteEngineStamp(path, engine);

    obfuscate_key = std::vector<unsigned char>(OBFUSCATE_KEY_NUM_BYTES, '\000');

    bool key_exists = Read(OBFUSCATE_KEY_KEY, obfuscate_key);

    if (!key_exists && obfuscate && IsEmpty()) {
        std::vector<unsigned char> new_key = CreateObfuscateKey();
        Write(OBFUSCATE_KEY_KEY, new_key);
        obfuscate_key = new_key;
        LogPrintf("Wrote new obfuscate key for %s: %s\n", path.string(), HexStr(obfuscate_key));
    }

    LogPrintf("Using obfuscation key for %s: %s\n", path.string(), HexStr(obfuscate_key));
    LogPrintf("KV engine for %s: %s (local only; not consensus)\n", path.string(), DbEngineName(engine));
}

CDBWrapper::~CDBWrapper()
{
    backend.reset();
}

bool CDBWrapper::WriteBatch(CDBBatch& batch, bool fSync)
{
    return backend->Write(batch.ops, fSync);
}

const std::string CDBWrapper::OBFUSCATE_KEY_KEY("\000obfuscate_key", 14);

const unsigned int CDBWrapper::OBFUSCATE_KEY_NUM_BYTES = 8;

std::vector<unsigned char> CDBWrapper::CreateObfuscateKey() const
{
    unsigned char buff[OBFUSCATE_KEY_NUM_BYTES];
    GetRandBytes(buff, OBFUSCATE_KEY_NUM_BYTES);
    return std::vector<unsigned char>(&buff[0], &buff[OBFUSCATE_KEY_NUM_BYTES]);
}

bool CDBWrapper::IsEmpty()
{
    std::unique_ptr<CDBIterator> it(NewIterator());
    it->SeekToFirst();
    return !(it->Valid());
}

CDBIterator::~CDBIterator() { delete piter; }
bool CDBIterator::Valid() { return piter && piter->Valid(); }
void CDBIterator::SeekToFirst() { piter->SeekToFirst(); }
void CDBIterator::Next() { piter->Next(); }

namespace dbwrapper_private {

const std::vector<unsigned char>& GetObfuscateKey(const CDBWrapper &w)
{
    return w.obfuscate_key;
}

};
