// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/walletdb_backend.h"

#include "fs.h"
#include "support/cleanse.h"
#include "util.h"
#include "wallet/db.h"

#include <boost/algorithm/string.hpp>

#include <stdexcept>

bool ParseWalletFormatPreference(const std::string& value, WalletFormatPreference& out, std::string& err)
{
    err.clear();
    std::string v = value;
    boost::algorithm::to_lower(v);
    if (v.empty() || v == "auto") {
        out = WalletFormatPreference::AUTO;
        return true;
    }
    if (v == "bdb" || v == "berkeley" || v == "berkeleydb") {
        out = WalletFormatPreference::BDB;
        return true;
    }
    if (v == "sqlite" || v == "sqlite3") {
        out = WalletFormatPreference::SQLITE;
        return true;
    }
    err = "unknown -walletformat value '" + value + "' (expected auto, bdb, or sqlite)";
    return false;
}

std::string WalletFormatPreferenceToString(WalletFormatPreference pref)
{
    switch (pref) {
    case WalletFormatPreference::BDB:
        return "bdb";
    case WalletFormatPreference::SQLITE:
        return "sqlite";
    case WalletFormatPreference::AUTO:
    default:
        return "auto";
    }
}

bool ResolveWalletDatabaseFormat(WalletFormatPreference pref,
                                 WalletDatabaseFormat detected,
                                 WalletDatabaseFormat& out_effective,
                                 std::string& err)
{
    err.clear();

    // Explicit preference for a backend we do not implement yet.
    if (pref == WalletFormatPreference::SQLITE) {
        err = "SQLite wallet backend is not implemented in this build; use -walletformat=bdb (default).";
        return false;
    }

    // On-disk SQLite is never openable yet.
    if (detected == WalletDatabaseFormat::SQLITE) {
        err = "Wallet file is SQLite format, which cannot be loaded yet. "
              "Use a Berkeley DB wallet.dat or wait for dual-stack migration support.";
        return false;
    }

    if (pref == WalletFormatPreference::BDB) {
        // Force BDB path for new or existing non-sqlite files.
        out_effective = WalletDatabaseFormat::BERKELEY;
        return true;
    }

    // AUTO
    if (detected == WalletDatabaseFormat::UNKNOWN) {
        // New wallet: create as BDB until SQLite create path exists.
        out_effective = WalletDatabaseFormat::BERKELEY;
        return true;
    }

    if (detected == WalletDatabaseFormat::BERKELEY) {
        out_effective = WalletDatabaseFormat::BERKELEY;
        return true;
    }

    err = "Unable to resolve wallet database format.";
    return false;
}

namespace {

/**
 * Subclass of CDB solely so we can reach protected raw BDB handles and
 * implement stream-level I/O for DatabaseBatch without changing CDB's public API.
 */
class BerkeleyDBAccess : public CDB
{
public:
    BerkeleyDBAccess(const std::string& filename, const char* pszMode, bool fFlushOnClose)
        : CDB(filename, pszMode, fFlushOnClose)
    {
    }

    bool ReadRaw(CDataStream& ssKey, CDataStream& ssValue)
    {
        if (!pdb)
            return false;

        Dbt datKey(ssKey.data(), ssKey.size());
        Dbt datValue;
        datValue.set_flags(DB_DBT_MALLOC);
        int ret = pdb->get(activeTxn, &datKey, &datValue, 0);
        memory_cleanse(datKey.get_data(), datKey.get_size());
        bool success = false;
        if (datValue.get_data() != NULL) {
            try {
                ssValue.clear();
                ssValue.SetType(SER_DISK);
                ssValue.write((char*)datValue.get_data(), datValue.get_size());
                success = true;
            } catch (const std::exception&) {
                // success remains false
            }
            memory_cleanse(datValue.get_data(), datValue.get_size());
            free(datValue.get_data());
        }
        return ret == 0 && success;
    }

    bool WriteRaw(CDataStream& ssKey, CDataStream& ssValue, bool fOverwrite)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");

        Dbt datKey(ssKey.data(), ssKey.size());
        Dbt datValue(ssValue.data(), ssValue.size());
        int ret = pdb->put(activeTxn, &datKey, &datValue, (fOverwrite ? 0 : DB_NOOVERWRITE));
        memory_cleanse(datKey.get_data(), datKey.get_size());
        memory_cleanse(datValue.get_data(), datValue.get_size());
        return (ret == 0);
    }

    bool EraseRaw(CDataStream& ssKey)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");

        Dbt datKey(ssKey.data(), ssKey.size());
        int ret = pdb->del(activeTxn, &datKey, 0);
        memory_cleanse(datKey.get_data(), datKey.get_size());
        return (ret == 0 || ret == DB_NOTFOUND);
    }

    bool ExistsRaw(CDataStream& ssKey)
    {
        if (!pdb)
            return false;

        Dbt datKey(ssKey.data(), ssKey.size());
        int ret = pdb->exists(activeTxn, &datKey, 0);
        memory_cleanse(datKey.get_data(), datKey.get_size());
        return (ret == 0);
    }

    Dbc* OpenCursor() { return GetCursor(); }

    int ReadAtCursorPublic(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, bool setRange)
    {
        return ReadAtCursor(pcursor, ssKey, ssValue, setRange);
    }

    bool BeginTxn() { return TxnBegin(); }
    bool CommitTxn() { return TxnCommit(); }
    bool AbortTxn() { return TxnAbort(); }
    void DoFlush() { Flush(); }
    void DoClose() { Close(); }
};

class BerkeleyCursor : public DatabaseCursor
{
    BerkeleyDBAccess& m_db;
    Dbc* m_cursor;

public:
    BerkeleyCursor(BerkeleyDBAccess& db, Dbc* cursor) : m_db(db), m_cursor(cursor) {}

    ~BerkeleyCursor()
    {
        Close();
    }

    DatabaseCursorStatus Read(CDataStream& ssKey, CDataStream& ssValue, bool setRange) override
    {
        if (!m_cursor)
            return DatabaseCursorStatus::FAIL;
        int ret = m_db.ReadAtCursorPublic(m_cursor, ssKey, ssValue, setRange);
        if (ret == 0)
            return DatabaseCursorStatus::MORE;
        if (ret == DB_NOTFOUND)
            return DatabaseCursorStatus::DONE;
        return DatabaseCursorStatus::FAIL;
    }

    void Close()
    {
        if (m_cursor) {
            m_cursor->close();
            m_cursor = NULL;
        }
    }
};

class BerkeleyBatch : public DatabaseBatch
{
    BerkeleyDBAccess m_db;

public:
    BerkeleyBatch(const std::string& filename, const char* pszMode, bool fFlushOnClose)
        : m_db(filename, pszMode, fFlushOnClose)
    {
    }

    bool ReadRaw(CDataStream& key, CDataStream& value) override
    {
        return m_db.ReadRaw(key, value);
    }

    bool WriteRaw(CDataStream& key, CDataStream& value, bool fOverwrite) override
    {
        return m_db.WriteRaw(key, value, fOverwrite);
    }

    bool EraseRaw(CDataStream& key) override
    {
        return m_db.EraseRaw(key);
    }

    bool ExistsRaw(CDataStream& key) override
    {
        return m_db.ExistsRaw(key);
    }

    std::unique_ptr<DatabaseCursor> GetNewCursor() override
    {
        Dbc* pcursor = m_db.OpenCursor();
        if (!pcursor)
            return std::unique_ptr<DatabaseCursor>();
        return std::unique_ptr<DatabaseCursor>(new BerkeleyCursor(m_db, pcursor));
    }

    bool TxnBegin() override { return m_db.BeginTxn(); }
    bool TxnCommit() override { return m_db.CommitTxn(); }
    bool TxnAbort() override { return m_db.AbortTxn(); }
    void Flush() override { m_db.DoFlush(); }
    void Close() override { m_db.DoClose(); }

    WalletDatabaseFormat GetFormat() const override
    {
        return WalletDatabaseFormat::BERKELEY;
    }
};

} // namespace

std::unique_ptr<DatabaseBatch> CreateWalletDatabaseBatch(
    const std::string& filename,
    const char* pszMode,
    bool fFlushOnClose,
    WalletDatabaseFormat format,
    std::string& err)
{
    err.clear();
    if (format == WalletDatabaseFormat::SQLITE) {
        err = "SQLite wallet backend is not implemented in this build";
        return std::unique_ptr<DatabaseBatch>();
    }
    if (format != WalletDatabaseFormat::BERKELEY) {
        err = "Unsupported wallet database format for open";
        return std::unique_ptr<DatabaseBatch>();
    }

    try {
        return std::unique_ptr<DatabaseBatch>(new BerkeleyBatch(filename, pszMode, fFlushOnClose));
    } catch (const std::exception& e) {
        err = e.what();
        return std::unique_ptr<DatabaseBatch>();
    }
}

std::unique_ptr<DatabaseBatch> CreateWalletDatabaseBatchFromPreference(
    const std::string& filename,
    const char* pszMode,
    bool fFlushOnClose,
    WalletFormatPreference pref,
    std::string& err)
{
    err.clear();
    fs::path path = filename.empty() ? fs::path() : (GetDataDir() / filename);
    WalletDatabaseFormat detected = filename.empty()
        ? WalletDatabaseFormat::UNKNOWN
        : DetectWalletDatabaseFormat(path);

    WalletDatabaseFormat effective;
    if (!ResolveWalletDatabaseFormat(pref, detected, effective, err))
        return std::unique_ptr<DatabaseBatch>();

    return CreateWalletDatabaseBatch(filename, pszMode, fFlushOnClose, effective, err);
}
