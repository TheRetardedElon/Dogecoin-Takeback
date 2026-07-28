// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/walletdb_format.h"
#include "wallet/walletdb_backend.h"
#include "test/test_dogecoin.h"
#include "fs.h"

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <fstream>
#include <string>

BOOST_FIXTURE_TEST_SUITE(walletdb_format_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(detect_missing_file_is_unknown)
{
    fs::path missing = pathTemp / "does-not-exist-wallet.dat";
    BOOST_CHECK(DetectWalletDatabaseFormat(missing) == WalletDatabaseFormat::UNKNOWN);
    BOOST_CHECK(WalletDatabaseFormatToString(WalletDatabaseFormat::UNKNOWN) == "unknown");
}

BOOST_AUTO_TEST_CASE(detect_sqlite_magic)
{
    fs::path path = pathTemp / "sqlite-wallet.dat";
    {
        std::ofstream out(path.string().c_str(), std::ios::binary);
        // SQLite header is 16 bytes: "SQLite format 3\0"
        const char hdr[16] = {'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3','\0'};
        out.write(hdr, 16);
        out.write("padding", 7);
    }
    BOOST_CHECK(DetectWalletDatabaseFormat(path) == WalletDatabaseFormat::SQLITE);
    BOOST_CHECK(WalletDatabaseFormatToString(WalletDatabaseFormat::SQLITE) == "sqlite");
    BOOST_CHECK(!IsWalletDatabaseFormatSupported(WalletDatabaseFormat::SQLITE));
    fs::remove(path);
}

BOOST_AUTO_TEST_CASE(detect_non_sqlite_as_berkeley)
{
    fs::path path = pathTemp / "bdb-ish-wallet.dat";
    {
        std::ofstream out(path.string().c_str(), std::ios::binary);
        // Any non-SQLite content is treated as BDB for detection purposes.
        const unsigned char blob[] = {0x00, 0x06, 0x15, 0x61, 0x01, 0x02, 0x03, 0x04};
        out.write(reinterpret_cast<const char*>(blob), sizeof(blob));
    }
    BOOST_CHECK(DetectWalletDatabaseFormat(path) == WalletDatabaseFormat::BERKELEY);
    BOOST_CHECK(IsWalletDatabaseFormatSupported(WalletDatabaseFormat::BERKELEY));
    fs::remove(path);
}

BOOST_AUTO_TEST_CASE(parse_wallet_format_preference)
{
    WalletFormatPreference pref;
    std::string err;
    BOOST_CHECK(ParseWalletFormatPreference("auto", pref, err));
    BOOST_CHECK(pref == WalletFormatPreference::AUTO);
    BOOST_CHECK(ParseWalletFormatPreference("BDB", pref, err));
    BOOST_CHECK(pref == WalletFormatPreference::BDB);
    BOOST_CHECK(ParseWalletFormatPreference("sqlite", pref, err));
    BOOST_CHECK(pref == WalletFormatPreference::SQLITE);
    BOOST_CHECK(!ParseWalletFormatPreference("rocksdb", pref, err));
    BOOST_CHECK(!err.empty());
}

BOOST_AUTO_TEST_CASE(resolve_wallet_format_rules)
{
    WalletDatabaseFormat effective;
    std::string err;

    // SQLite preference not implemented
    BOOST_CHECK(!ResolveWalletDatabaseFormat(WalletFormatPreference::SQLITE,
                                             WalletDatabaseFormat::UNKNOWN, effective, err));
    BOOST_CHECK(!err.empty());

    // Detected SQLite refused
    err.clear();
    BOOST_CHECK(!ResolveWalletDatabaseFormat(WalletFormatPreference::AUTO,
                                             WalletDatabaseFormat::SQLITE, effective, err));

    // AUTO + missing => BDB create path
    err.clear();
    BOOST_CHECK(ResolveWalletDatabaseFormat(WalletFormatPreference::AUTO,
                                            WalletDatabaseFormat::UNKNOWN, effective, err));
    BOOST_CHECK(effective == WalletDatabaseFormat::BERKELEY);

    // AUTO + BDB file => BDB
    err.clear();
    BOOST_CHECK(ResolveWalletDatabaseFormat(WalletFormatPreference::AUTO,
                                            WalletDatabaseFormat::BERKELEY, effective, err));
    BOOST_CHECK(effective == WalletDatabaseFormat::BERKELEY);
}

BOOST_AUTO_TEST_SUITE_END()
