// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_WALLET_WALLETDB_FORMAT_H
#define DOGECOIN_WALLET_WALLETDB_FORMAT_H

#include "fs.h"

#include <string>

/**
 * Wallet on-disk database backend identifier.
 *
 * Today Dogecoin Core only opens Berkeley DB wallets (wallet.dat).
 * SQLite detection is present so we can refuse foreign formats safely
 * and migrate later without guessing file type.
 */
enum class WalletDatabaseFormat {
    BERKELEY, //!< BDB 4.8/5.x wallet.dat (current production format)
    SQLITE,   //!< SQLite wallet (planned dual-stack; not loadable yet)
    UNKNOWN,  //!< Empty/missing/unrecognized
};

/** Probe path and return detected format (does not open a DB session). */
WalletDatabaseFormat DetectWalletDatabaseFormat(const fs::path& wallet_path);

/** Human-readable name for RPC/logs: "bdb", "sqlite", "unknown". */
std::string WalletDatabaseFormatToString(WalletDatabaseFormat fmt);

/**
 * True if this process can open the format for full wallet use.
 * Currently only BERKELEY (and UNKNOWN empty path for new wallet create).
 */
bool IsWalletDatabaseFormatSupported(WalletDatabaseFormat fmt);

#endif // DOGECOIN_WALLET_WALLETDB_FORMAT_H
