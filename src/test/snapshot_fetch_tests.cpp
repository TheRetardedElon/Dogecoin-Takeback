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
    BOOST_REQUIRE(ParseSnapshotArtifactManifest(json, m, err));
    BOOST_CHECK_EQUAL(m.height, 100);
    BOOST_CHECK_EQUAL(m.url, "https://example.com/utxo.dat");
    BOOST_CHECK_EQUAL(m.size_bytes, 1234);
}

BOOST_AUTO_TEST_CASE(parse_gpe_latest_json_aliases)
{
    // Shape used by sync.doge.gopastearth.com / make_utxo_snapshot.sh
    std::string json =
        "{"
        "\"network\": \"main\","
        "\"hostname\": \"sync.doge.gopastearth.com\","
        "\"url\": \"https://sync.doge.gopastearth.com/utxo-6324326-20260809.dat\","
        "\"filename\": \"utxo-6324326-20260809.dat\","
        "\"sha256\": \"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
        "\"bytes\": 987654321,"
        "\"blocks\": 6324326,"
        "\"bestblock\": \"abcd\","
        "\"hash_serialized\": \"ef01\","
        "\"created_utc\": \"2026-08-09T00:00:00Z\","
        "\"producer\": \"gpednode1\""
        "}";
    SnapshotArtifactManifest m;
    std::string err;
    BOOST_REQUIRE_MESSAGE(ParseSnapshotArtifactManifest(json, m, err), err);
    BOOST_CHECK_EQUAL(m.height, 6324326);
    BOOST_CHECK_EQUAL(m.size_bytes, 987654321);
    BOOST_CHECK_EQUAL(m.url, "https://sync.doge.gopastearth.com/utxo-6324326-20260809.dat");
    BOOST_CHECK_EQUAL(m.artifact_sha256_hex,
                      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    BOOST_CHECK_EQUAL(m.base_blockhash_hex, "abcd");
    BOOST_CHECK_EQUAL(m.hash_serialized_hex, "ef01");

    // Resolve from inline JSON (no network)
    SnapshotArtifactManifest m2;
    BOOST_REQUIRE(ResolveSnapshotFromManifest(json, m2, err));
    BOOST_CHECK_EQUAL(m2.height, 6324326);
    BOOST_CHECK(LooksLikeJsonObject(json));
    BOOST_CHECK(!LooksLikeJsonObject("https://sync.doge.gopastearth.com/latest.json"));
}

BOOST_AUTO_TEST_CASE(parse_gpe_placeholder_awaiting)
{
    std::string json =
        "{"
        "\"status\": \"awaiting first snapshot\","
        "\"nodeTip\": {\"blocks\": 6324326}"
        "}";
    SnapshotArtifactManifest m;
    std::string err;
    BOOST_CHECK(!ParseSnapshotArtifactManifest(json, m, err));
    BOOST_CHECK(err.find("not published") != std::string::npos ||
                err.find("awaiting") != std::string::npos ||
                err.find("status") != std::string::npos);
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
