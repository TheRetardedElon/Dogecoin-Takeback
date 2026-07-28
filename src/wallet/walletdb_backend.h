// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_WALLET_WALLETDB_BACKEND_H
#define DOGECOIN_WALLET_WALLETDB_BACKEND_H

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "wallet/walletdb_format.h"

#include <memory>
#include <string>

/**
 * Dual-stack wallet database backend contract (Phase 5B).
 *
 * Production I/O still uses Berkeley DB. CWalletDB now owns a DatabaseBatch
 * (composition) instead of inheriting CDB, so a future SQLite batch can plug
 * in without rewriting wallet.cpp call sites.
 *
 * Layout:
 *   - DatabaseCursor         — sequential key/value scan (backend-agnostic)
 *   - DatabaseBatch          — RAII read/write/erase/txn/cursor for one open
 *   - BerkeleyBatch          — wraps existing CDBEnv/CDB (current path, in .cpp)
 *   - SQLiteBatch            — future ENABLE_SQLITE implementation
 *
 * Migration rule: never destroy BDB wallet.dat until migratewallet has verified
 * a successful SQLite copy and the user opts in.
 */

/** Operator-facing format preference from -walletformat= */
enum class WalletFormatPreference {
    AUTO,    //!< Detect from file; create new as BDB until SQLite lands
    BDB,     //!< Require / create Berkeley DB
    SQLITE,  //!< Require / create SQLite (not fully implemented yet)
};

/**
 * Parse -walletformat value: "auto"|"bdb"|"berkeley"|"sqlite".
 * Empty => AUTO. Returns false on unrecognized tokens.
 */
bool ParseWalletFormatPreference(const std::string& value, WalletFormatPreference& out, std::string& err);

/** String for logs/RPC. */
std::string WalletFormatPreferenceToString(WalletFormatPreference pref);

/**
 * Resolve preference against on-disk detection.
 * Returns false and sets err if the combination is invalid for this build.
 *
 * Rules (current build):
 *  - SQLITE preference always fails (backend not implemented).
 *  - Detected SQLITE on disk always fails (cannot load).
 *  - AUTO + missing file => BERKELEY create path.
 *  - BDB + BDB/UNKNOWN file => ok.
 */
bool ResolveWalletDatabaseFormat(WalletFormatPreference pref,
                                 WalletDatabaseFormat detected,
                                 WalletDatabaseFormat& out_effective,
                                 std::string& err);

/** Result of one cursor step (portable; no BDB constants in this header). */
enum class DatabaseCursorStatus {
    MORE, //!< Record available in key/value streams
    DONE, //!< End of iteration (or past range)
    FAIL  //!< I/O or cursor error
};

/**
 * Backend-agnostic sequential scan over wallet key/value records.
 * Destroying the cursor closes the underlying backend cursor.
 */
class DatabaseCursor
{
public:
    virtual ~DatabaseCursor() {}

    /**
     * Advance cursor. If setRange is true, seek to the first key >= ssKey
     * (contents of ssKey on entry are the seek prefix) and return that record.
     * On MORE, ssKey/ssValue are filled with the record. On DONE/FAIL they are
     * unspecified.
     */
    virtual DatabaseCursorStatus Read(CDataStream& ssKey, CDataStream& ssValue, bool setRange = false) = 0;
};

/**
 * Backend-agnostic batch handle for wallet database I/O.
 *
 * Typed Read/Write/Erase/Exists serialize with SER_DISK / CLIENT_VERSION the
 * same way CDB historically did, then call the raw stream virtuals.
 */
class DatabaseBatch
{
public:
    virtual ~DatabaseBatch() {}

    virtual bool ReadRaw(CDataStream& key, CDataStream& value) = 0;
    virtual bool WriteRaw(CDataStream& key, CDataStream& value, bool fOverwrite = true) = 0;
    virtual bool EraseRaw(CDataStream& key) = 0;
    virtual bool ExistsRaw(CDataStream& key) = 0;

    /** New sequential cursor, or nullptr on failure. */
    virtual std::unique_ptr<DatabaseCursor> GetNewCursor() = 0;

    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual bool TxnAbort() = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;

    virtual WalletDatabaseFormat GetFormat() const = 0;

    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        if (!ReadRaw(ssKey, ssValue))
            return false;
        try {
            ssValue >> value;
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;
        return WriteRaw(ssKey, ssValue, fOverwrite);
    }

    template <typename K>
    bool Erase(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        return EraseRaw(ssKey);
    }

    template <typename K>
    bool Exists(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        return ExistsRaw(ssKey);
    }

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }

    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }
};

/**
 * Open a batch for the given wallet file and resolved format.
 * Only BERKELEY is implemented; other formats set err and return nullptr.
 */
std::unique_ptr<DatabaseBatch> CreateWalletDatabaseBatch(
    const std::string& filename,
    const char* pszMode,
    bool fFlushOnClose,
    WalletDatabaseFormat format,
    std::string& err);

/**
 * Detect on-disk format, resolve preference, then open.
 * Fails closed for SQLite preference or SQLite files (this build).
 */
std::unique_ptr<DatabaseBatch> CreateWalletDatabaseBatchFromPreference(
    const std::string& filename,
    const char* pszMode,
    bool fFlushOnClose,
    WalletFormatPreference pref,
    std::string& err);

#endif // DOGECOIN_WALLET_WALLETDB_BACKEND_H
