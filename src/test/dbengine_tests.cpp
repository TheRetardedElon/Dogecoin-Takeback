// Copyright (c) 2026 The Dogecoin Core Pro developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbengine.h"
#include "dbmigrate.h"
#include "dbwrapper.h"
#include "fs.h"
#include "test/test_dogecoin.h"

#include <fstream>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(dbengine_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(parse_names)
{
    DbEngine e;
    BOOST_CHECK(ParseDbEngine("leveldb", e) && e == DbEngine::LEVELDB);
    BOOST_CHECK(ParseDbEngine("LEVELDB", e) && e == DbEngine::LEVELDB);
    BOOST_CHECK(ParseDbEngine("mdbx", e) && e == DbEngine::MDBX);
    BOOST_CHECK(ParseDbEngine("libmdbx", e) && e == DbEngine::MDBX);
    BOOST_CHECK(!ParseDbEngine("rocksdb", e) && e == DbEngine::UNKNOWN);
}

BOOST_AUTO_TEST_CASE(detect_empty_and_stamp)
{
    fs::path ph = fs::temp_directory_path() / fs::unique_path();
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::NONE);
    BOOST_CHECK(WriteEngineStamp(ph, DbEngine::LEVELDB));
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::LEVELDB);
    fs::remove_all(ph);
}

BOOST_AUTO_TEST_CASE(detect_legacy_leveldb_current)
{
    fs::path ph = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(ph);
    std::ofstream f((ph / "CURRENT").string().c_str());
    f << "MANIFEST-000001\n";
    f.close();
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::LEVELDB);
    fs::remove_all(ph);
}

BOOST_AUTO_TEST_CASE(stale_stamp_without_files_is_none)
{
    fs::path ph = fs::temp_directory_path() / fs::unique_path();
    BOOST_CHECK(WriteEngineStamp(ph, DbEngine::LEVELDB));
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::NONE);
    BOOST_CHECK(WriteEngineStamp(ph, DbEngine::MDBX));
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::NONE);
    fs::remove_all(ph);
}

BOOST_AUTO_TEST_CASE(wrapper_stamps_leveldb_on_disk)
{
    fs::path ph = fs::temp_directory_path() / fs::unique_path();
    {
        CDBWrapper dbw(ph, (1 << 20), false, false, false);
        BOOST_CHECK(dbw.GetEngine() == DbEngine::LEVELDB);
        char key = 'k';
        int in = 42;
        int out = 0;
        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, out));
        BOOST_CHECK_EQUAL(out, 42);
    }
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::LEVELDB);
    fs::remove_all(ph);
}

BOOST_AUTO_TEST_CASE(mdbx_roundtrip_empty_dir)
{
    fs::path ph = fs::temp_directory_path() / fs::unique_path();
    std::string err;
    CDbBackend* b = CreateDbBackend(DbEngine::MDBX, ph, 1 << 20, false, false, err);
    BOOST_REQUIRE_MESSAGE(b != NULL, err);
    {
        std::vector<DbBatchOp> ops;
        DbBatchOp op;
        op.erase = false;
        op.key = "k";
        op.value = "v1";
        ops.push_back(op);
        BOOST_CHECK(b->Write(ops, true));
        std::string out;
        BOOST_CHECK(b->Get("k", out));
        BOOST_CHECK_EQUAL(out, "v1");
    }
    delete b;
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::MDBX ||
                DetectExistingEngine(ph) == DbEngine::NONE);
    // stamp is written by CDBWrapper, not CreateDbBackend
    fs::remove_all(ph);
}

BOOST_AUTO_TEST_CASE(select_honors_stamp_when_flag_unset)
{
    fs::path ph = fs::temp_directory_path() / fs::unique_path();
    {
        CDBWrapper dbw(ph, (1 << 20), false, false, false, DbEngine::MDBX);
        BOOST_CHECK(dbw.GetEngine() == DbEngine::MDBX);
    }
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::MDBX);
    // No override and no -dbengine: stamp wins.
    {
        CDBWrapper dbw(ph, (1 << 20), false, false, false);
        BOOST_CHECK(dbw.GetEngine() == DbEngine::MDBX);
    }
    fs::remove_all(ph);
}

BOOST_AUTO_TEST_CASE(wrapper_mdbx_and_refuse_leveldb_reopen)
{
    fs::path ph = fs::temp_directory_path() / fs::unique_path();
    {
        CDBWrapper dbw(ph, (1 << 20), false, false, false, DbEngine::MDBX);
        BOOST_CHECK(dbw.GetEngine() == DbEngine::MDBX);
        char key = 'k';
        int in = 7;
        int out = 0;
        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, out));
        BOOST_CHECK_EQUAL(out, 7);
    }
    BOOST_CHECK(DetectExistingEngine(ph) == DbEngine::MDBX);
    BOOST_CHECK_THROW(CDBWrapper(ph, (1 << 20), false, false, false, DbEngine::LEVELDB), dbwrapper_error);
    fs::remove_all(ph);
}

BOOST_AUTO_TEST_CASE(copy_leveldb_to_mdbx_raw_bytes)
{
    fs::path src = fs::temp_directory_path() / fs::unique_path();
    fs::path dst = fs::temp_directory_path() / fs::unique_path();
    {
        CDBWrapper dbw(src, (1 << 20), false, false, true, DbEngine::LEVELDB);
        for (int i = 0; i < 50; ++i)
            BOOST_CHECK(dbw.Write(i, i * 3));
    }
    uint64_t n = 0;
    std::string err;
    BOOST_REQUIRE_MESSAGE(CopyKvDirectory(src, DbEngine::LEVELDB, dst, DbEngine::MDBX, n, err), err);
    BOOST_CHECK(n >= 50);
    {
        CDBWrapper dbw(dst, (1 << 20), false, false, true, DbEngine::MDBX);
        BOOST_CHECK(dbw.GetEngine() == DbEngine::MDBX);
        int out = 0;
        BOOST_CHECK(dbw.Read(0, out));
        BOOST_CHECK_EQUAL(out, 0);
        BOOST_CHECK(dbw.Read(7, out));
        BOOST_CHECK_EQUAL(out, 21);
    }
    BOOST_CHECK(!CopyKvDirectory(src, DbEngine::LEVELDB, dst, DbEngine::MDBX, n, err));
    fs::remove_all(src);
    fs::remove_all(dst);
}

BOOST_AUTO_TEST_SUITE_END()
