// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/walletdb_format.h"

#include <cstdio>
#include <cstring>

namespace {

// SQLite database files begin with this 16-byte header (NUL-terminated string).
// See https://www.sqlite.org/fileformat.html
const char SQLITE_HEADER[] = "SQLite format 3";

} // namespace

WalletDatabaseFormat DetectWalletDatabaseFormat(const fs::path& wallet_path)
{
    if (!fs::exists(wallet_path) || !fs::is_regular_file(wallet_path)) {
        // New wallet path: create as Berkeley until SQLite dual-stack lands.
        return WalletDatabaseFormat::UNKNOWN;
    }

    if (fs::file_size(wallet_path) == 0) {
        return WalletDatabaseFormat::UNKNOWN;
    }

    unsigned char header[16] = {0};
    FILE* f = fsbridge::fopen(wallet_path, "rb");
    if (!f) {
        return WalletDatabaseFormat::UNKNOWN;
    }
    const size_t got = fread(header, 1, sizeof(header), f);
    fclose(f);
    if (got == 0) {
        return WalletDatabaseFormat::UNKNOWN;
    }

    // Full SQLite header requires 16 bytes starting with "SQLite format 3\0".
    if (got >= 16 && memcmp(header, SQLITE_HEADER, 15) == 0 && header[15] == '\0') {
        return WalletDatabaseFormat::SQLITE;
    }

    // Non-SQLite existing wallet files are treated as Berkeley DB.
    // BDB magic varies by page size/endian; full open still goes through bitdb.
    return WalletDatabaseFormat::BERKELEY;
}

std::string WalletDatabaseFormatToString(WalletDatabaseFormat fmt)
{
    switch (fmt) {
    case WalletDatabaseFormat::BERKELEY:
        return "bdb";
    case WalletDatabaseFormat::SQLITE:
        return "sqlite";
    case WalletDatabaseFormat::UNKNOWN:
    default:
        return "unknown";
    }
}

bool IsWalletDatabaseFormatSupported(WalletDatabaseFormat fmt)
{
    // Phase 5 dual-stack: only BDB is writable/loadable today.
    // UNKNOWN is allowed for brand-new wallet.dat creation via BDB.
    return fmt == WalletDatabaseFormat::BERKELEY || fmt == WalletDatabaseFormat::UNKNOWN;
}
