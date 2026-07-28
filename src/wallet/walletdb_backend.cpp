// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/walletdb_backend.h"

#include <boost/algorithm/string.hpp>

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
