// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_WALLET_WALLETDB_BACKEND_H
#define DOGECOIN_WALLET_WALLETDB_BACKEND_H

#include "wallet/walletdb_format.h"

#include <string>

/**
 * Dual-stack wallet database backend contract (Phase 5B).
 *
 * Today all production I/O still goes through Berkeley DB via CDB/CWalletDB.
 * This header defines the *target* abstraction so SQLite can be added without
 * rewriting wallet.cpp call sites in one shot.
 *
 * Planned layout:
 *   - WalletDatabaseBackend  — open/close/flush environment for one wallet file
 *   - WalletDatabaseBatch    — RAII read/write/erase/cursor for key/value records
 *   - BerkeleyBackend        — wraps existing CDBEnv/CDB (current path)
 *   - SQLiteBackend          — future ENABLE_SQLITE implementation
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

#endif // DOGECOIN_WALLET_WALLETDB_BACKEND_H
