// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_NODE_SNAPSHOT_FETCH_H
#define DOGECOIN_NODE_SNAPSHOT_FETCH_H

#include "fs.h"
#include "uint256.h"

#include <string>

/**
 * Product P1 helpers: treat cloud/CDN as a dumb pipe.
 * Stream bytes to a local file while hashing (no full-file RAM load).
 * Fail closed on digest mismatch — never trust the transport.
 *
 * artifact_sha256 is single SHA-256 of file bytes (sha256sum style),
 * NOT double-SHA256 and NOT hash_serialized (UTXO set hash).
 */

/** Stream-hash an existing local file with single SHA-256. */
bool HashFileSha256(const fs::path& path, uint256& hash_out, uint64_t& bytes_out, std::string& error);

/**
 * Parse 64-char hex into uint256 (big-endian display order same as GetHex).
 * Returns false if not valid hex of correct length.
 */
bool ParseSha256Hex(const std::string& hex, uint256& out, std::string& error);

/**
 * Copy local source to dest while hashing (stream). Overwrites dest on success only
 * (writes via dest.partial then renames).
 */
bool CopyFileStreamHash(const fs::path& source,
                        const fs::path& dest,
                        uint256& hash_out,
                        uint64_t& bytes_out,
                        std::string& error);

/**
 * HTTP(S) GET url → dest while hashing.
 * Uses libevent. Follows no custom auth. For https, requires platform SSL support
 * in libevent (same constraints as other HTTP client uses).
 *
 * On failure, dest partial is removed when possible.
 */
bool DownloadUrlStreamHash(const std::string& url,
                           const fs::path& dest,
                           uint256& hash_out,
                           uint64_t& bytes_out,
                           std::string& error,
                           int timeout_sec = 600);

/**
 * Download or copy into dest, then require hash_out == expected.
 * source may be:
 *   - https:// or http:// URL
 *   - file path (absolute or relative)
 * Fail closed: deletes dest on digest mismatch.
 */
bool FetchSnapshotArtifact(const std::string& source,
                           const fs::path& dest,
                           const uint256& expected_sha256,
                           uint64_t& bytes_out,
                           std::string& error,
                           int timeout_sec = 600);

/**
 * Optional CDN manifest fields (JSON). Product P1.
 * coins hash_serialized is checked later by loadtxoutset / mapAssumeutxo — not here.
 */
struct SnapshotArtifactManifest {
    int height;
    std::string base_blockhash_hex;
    std::string hash_serialized_hex; // UTXO set hash (informational in manifest)
    std::string artifact_sha256_hex; // required for fetch
    std::string url;
    int64_t size_bytes;

    SnapshotArtifactManifest() : height(-1), size_bytes(-1) {}
};

/** Parse a minimal JSON manifest object (univalue). */
bool ParseSnapshotArtifactManifest(const std::string& json, SnapshotArtifactManifest& out, std::string& error);

#endif // DOGECOIN_NODE_SNAPSHOT_FETCH_H
