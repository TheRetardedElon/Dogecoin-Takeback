// Copyright (c) 2012-2016 The Bitcoin Core developers
// Copyright (c) 2026 The Dogecoin Core Pro developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_DBWRAPPER_H
#define DOGECOIN_DBWRAPPER_H

#include "clientversion.h"
#include "dbengine.h"
#include "fs.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utilstrencodings.h"
#include "version.h"

#include <memory>
#include <string>
#include <vector>

static const size_t DBWRAPPER_PREALLOC_KEY_SIZE = 64;
static const size_t DBWRAPPER_PREALLOC_VALUE_SIZE = 1024;

class dbwrapper_error : public std::runtime_error
{
public:
    dbwrapper_error(const std::string& msg) : std::runtime_error(msg) {}
};

class CDBWrapper;

/** These should be considered an implementation detail of the specific database.
 */
namespace dbwrapper_private {

/** Work around circular dependency, as well as for testing in dbwrapper_tests.
 * Database obfuscation should be considered an implementation detail of the
 * specific database.
 */
const std::vector<unsigned char>& GetObfuscateKey(const CDBWrapper &w);

};

/** Batch of changes queued to be written to a CDBWrapper */
class CDBBatch
{
    friend class CDBWrapper;

private:
    const CDBWrapper &parent;
    std::vector<DbBatchOp> ops;

    CDataStream ssKey;
    CDataStream ssValue;

public:
    /**
     * @param[in] _parent   CDBWrapper that this batch is to be submitted to
     */
    CDBBatch(const CDBWrapper &_parent) : parent(_parent), ssKey(SER_DISK, CLIENT_VERSION), ssValue(SER_DISK, CLIENT_VERSION) { };

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        ssValue.reserve(DBWRAPPER_PREALLOC_VALUE_SIZE);
        ssValue << value;
        ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));

        DbBatchOp op;
        op.erase = false;
        op.key.assign(ssKey.begin(), ssKey.end());
        op.value.assign(ssValue.begin(), ssValue.end());
        ops.push_back(op);
        ssKey.clear();
        ssValue.clear();
    }

    template <typename K>
    void Erase(const K& key)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        DbBatchOp op;
        op.erase = true;
        op.key.assign(ssKey.begin(), ssKey.end());
        ops.push_back(op);
        ssKey.clear();
    }

    /** Already-serialized, already-obfuscated bytes (migration). */
    void WriteRaw(const std::string& key, const std::string& value)
    {
        DbBatchOp op;
        op.erase = false;
        op.key = key;
        op.value = value;
        ops.push_back(op);
    }

    void Clear() { ops.clear(); }
};

class CDBIterator
{
private:
    const CDBWrapper &parent;
    CDbBackendIterator* piter;

public:
    CDBIterator(const CDBWrapper &_parent, CDbBackendIterator *_piter) :
        parent(_parent), piter(_piter) { };
    ~CDBIterator();

    bool Valid();

    void SeekToFirst();

    template<typename K> void Seek(const K& key) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        piter->Seek(std::string(ssKey.begin(), ssKey.end()));
    }

    void Next();

    template<typename K> bool GetKey(K& key) {
        try {
            const std::string raw = piter->Key();
            CDataStream ssKey(raw.data(), raw.data() + raw.size(), SER_DISK, CLIENT_VERSION);
            ssKey >> key;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    unsigned int GetKeySize() {
        return (unsigned int)piter->Key().size();
    }

    template<typename V> bool GetValue(V& value) {
        try {
            const std::string raw = piter->Value();
            CDataStream ssValue(raw.data(), raw.data() + raw.size(), SER_DISK, CLIENT_VERSION);
            ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    unsigned int GetValueSize() {
        return (unsigned int)piter->Value().size();
    }

    /** Raw on-disk key/value (obfuscation already applied to values). */
    std::string GetRawKey() const { return piter->Key(); }
    std::string GetRawValue() const { return piter->Value(); }
};

class CDBWrapper
{
    friend const std::vector<unsigned char>& dbwrapper_private::GetObfuscateKey(const CDBWrapper &w);
private:
    std::unique_ptr<CDbBackend> backend;

    //! a key used for optional XOR-obfuscation of the database
    std::vector<unsigned char> obfuscate_key;

    //! the key under which the obfuscation key is stored
    static const std::string OBFUSCATE_KEY_KEY;

    //! the length of the obfuscate key in number of bytes
    static const unsigned int OBFUSCATE_KEY_NUM_BYTES;

    std::vector<unsigned char> CreateObfuscateKey() const;

    DbEngine engine = DbEngine::LEVELDB;

public:
    /**
     * @param[in] path        Location in the filesystem where data will be stored.
     * @param[in] nCacheSize  Configures backend cache settings.
     * @param[in] fMemory     If true, use an in-memory environment (LevelDB memenv).
     * @param[in] fWipe       If true, remove all existing data.
     * @param[in] obfuscate   If true, store data obfuscated via simple XOR. If false, XOR
     *                        with a zero'd byte array.
     */
    CDBWrapper(const fs::path& path, size_t nCacheSize, bool fMemory = false, bool fWipe = false, bool obfuscate = false, DbEngine engineOverride = DbEngine::NONE);
    ~CDBWrapper();

    DbEngine GetEngine() const { return engine; }

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        std::string strValue;
        if (!backend->Get(std::string(ssKey.begin(), ssKey.end()), strValue))
            return false;
        try {
            CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue.Xor(obfuscate_key);
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        CDBBatch batch(*this);
        batch.Write(key, value);
        return WriteBatch(batch, fSync);
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        std::string strValue;
        return backend->Get(std::string(ssKey.begin(), ssKey.end()), strValue);
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        CDBBatch batch(*this);
        batch.Erase(key);
        return WriteBatch(batch, fSync);
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false);

    // not available for LevelDB; provide for compatibility with BDB
    bool Flush()
    {
        return true;
    }

    bool Sync()
    {
        CDBBatch batch(*this);
        return WriteBatch(batch, true);
    }

    CDBIterator *NewIterator()
    {
        return new CDBIterator(*this, backend->NewIterator());
    }

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty();
};

#endif // DOGECOIN_DBWRAPPER_H
