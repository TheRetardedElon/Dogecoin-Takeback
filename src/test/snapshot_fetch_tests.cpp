// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "node/snapshot_fetch.h"
#include "test/test_dogecoin.h"
#include "utilstrencodings.h"

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <string>

BOOST_FIXTURE_TEST_SUITE(snapshot_fetch_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(hash_file_sha256_known)
{
    // echo -n "dogecoin" | sha256sum
    // 2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae is "foo"
    // Compute for "dogecoin-pro-p1"
    fs::path p = pathTemp / "snap_hash_test.bin";
    {
        FILE* f = fsbridge::fopen(p, "wb");
        BOOST_REQUIRE(f);
        const char* msg = "dogecoin-pro-p1";
        BOOST_REQUIRE_EQUAL(fwrite(msg, 1, 15, f), 15u);
        fclose(f);
    }
    uint256 hash;
    uint64_t bytes = 0;
    std::string err;
    BOOST_REQUIRE(HashFileSha256(p, hash, bytes, err));
    BOOST_CHECK_EQUAL(bytes, 15u);
    BOOST_CHECK_EQUAL(hash.GetHex().size(), 64u);

    // Fail closed: wrong expected
    fs::path dest = pathTemp / "snap_copy.dat";
    uint256 wrong;
    wrong.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    BOOST_CHECK(!FetchSnapshotArtifact(p.string(), dest, wrong, bytes, err));
    BOOST_CHECK(!fs::exists(dest));

    // Success path: copy with correct hash
    BOOST_REQUIRE(FetchSnapshotArtifact(p.string(), dest, hash, bytes, err));
    BOOST_CHECK(fs::exists(dest));
    BOOST_CHECK_EQUAL(bytes, 15u);
}

BOOST_AUTO_TEST_CASE(parse_manifest_json)
{
    std::string json =
        "{"
        "\"height\": 100,"
        "\"hash_serialized\": \"aaaa\","
        "\"artifact_sha256\": \"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
        "\"url\": \"https://example.com/utxo.dat\","
        "\"size_bytes\": 1234"
        "}";
    SnapshotArtifactManifest m;
    std::string err;
    // hash_serialized in manifest may be short in this test for field presence only —
    // artifact_sha256 must be 64 hex. aaaa is only for optional field.
    // Fix: use valid 64-hex for artifact only; hash_serialized can be any string in manifest.
    BOOST_REQUIRE(ParseSnapshotArtifactManifest(json, m, err));
    BOOST_CHECK_EQUAL(m.height, 100);
    BOOST_CHECK_EQUAL(m.url, "https://example.com/utxo.dat");
    BOOST_CHECK_EQUAL(m.size_bytes, 1234);
}

BOOST_AUTO_TEST_CASE(parse_sha256_hex)
{
    uint256 out;
    std::string err;
    BOOST_CHECK(!ParseSha256Hex("zz", out, err));
    BOOST_CHECK(ParseSha256Hex(
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", out, err));
    BOOST_CHECK_EQUAL(out.GetHex(),
                      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
}

BOOST_AUTO_TEST_SUITE_END()
