// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_NODE_SNAPSHOT_FETCH_H
#define DOGECOIN_NODE_SNAPSHOT_FETCH_H

#include "fs.h"
#include "uint256.h"

#include <string>
#include <vector>

/**
 * Product P1 helpers: treat cloud/CDN as a dumb pipe.
 * Stream bytes to a local file while hashing (no full-file RAM load).
 * Fail closed on digest mismatch — never trust the transport.
 *
 * artifact_sha256 is single SHA-256 of file bytes (sha256sum style),
 * NOT double-SHA256 and NOT hash_serialized (UTXO set hash).
 */

/**
 * Optional progress for multi-GB CDN downloads (GUI Fast Sync).
 * bytes_done = bytes written so far; expected_bytes = Content-Length or -1 if unknown.
 * Return false to abort the download (fail closed; partial file removed).
 */
typedef bool (*SnapshotProgressFn)(uint64_t bytes_done, int64_t expected_bytes, void* ctx);

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
 * Windows: WinHTTP + Schannel (full HTTPS for CDN Fast Sync).
 * Non-Windows: libevent (plain HTTP; HTTPS needs SSL-enabled libevent or local path).
 * Follows redirects on WinHTTP. No custom auth.
 *
 * On failure, dest partial is removed when possible.
 */
bool DownloadUrlStreamHash(const std::string& url,
                           const fs::path& dest,
                           uint256& hash_out,
                           uint64_t& bytes_out,
                           std::string& error,
                           int timeout_sec = 600,
                           SnapshotProgressFn progress = 0,
                           void* progress_ctx = 0);

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
                           int timeout_sec = 600,
                           SnapshotProgressFn progress = 0,
                           void* progress_ctx = 0);

/**
 * Mesh M2: try each source URL/path until one stream-hashes to expected_sha256.
 * Never accepts a different digest from a "faster" host. On total failure, error
 * lists each attempt. Partial files are removed on mismatch/abort.
 */
bool FetchSnapshotArtifactFromCandidates(const std::vector<std::string>& sources,
                                         const fs::path& dest,
                                         const uint256& expected_sha256,
                                         uint64_t& bytes_out,
                                         std::string& error,
                                         std::string* used_source_out = 0,
                                         int timeout_sec = 600,
                                         SnapshotProgressFn progress = 0,
                                         void* progress_ctx = 0);

/**
 * Optional CDN manifest fields (JSON). Product P1.
 * coins hash_serialized is checked later by loadtxoutset / mapAssumeutxo — not here.
 *
 * Compatible with GPE Fast Sync CDN latest.json (sync.doge.gopastearth.com):
 *   url, sha256|artifact_sha256, blocks|height, bytes|size_bytes, bestblock, hash_serialized, status
 */
struct SnapshotArtifactManifest {
    int height;
    std::string base_blockhash_hex;
    std::string hash_serialized_hex; // UTXO set hash (informational in manifest)
    std::string artifact_sha256_hex; // required for fetch
    std::string url;                 // primary URL (always set when valid)
    std::vector<std::string> urls;   // mesh M2: try in order; same sha256 for all
    int64_t size_bytes;
    std::string status; // e.g. "awaiting first snapshot" (not ready)

    SnapshotArtifactManifest() : height(-1), size_bytes(-1) {}

    /** Primary first, then remaining urls[] without duplicates. */
    std::vector<std::string> CandidateUrls() const;
};

/** Official GPE public manifest URL (empty until first dumptxoutset is published). */
static const char* const DEFAULT_SNAPSHOT_MANIFEST_URL =
    "https://sync.doge.gopastearth.com/latest.json";

/** Parse a minimal JSON manifest object (univalue). GPE field aliases accepted. */
bool ParseSnapshotArtifactManifest(const std::string& json, SnapshotArtifactManifest& out, std::string& error);

/**
 * Download a small text body (manifest JSON) via HTTP(S).
 * Windows: WinHTTP (HTTPS OK). Non-Windows: same constraints as DownloadUrlStreamHash.
 */
bool DownloadUrlToString(const std::string& url,
                         std::string& body_out,
                         std::string& error,
                         int timeout_sec = 60);

/**
 * Resolve artifact URL + SHA-256 from:
 *   - a JSON object string, or
 *   - an http(s) URL to latest.json (downloaded then parsed), or
 *   - a local path to a JSON file
 * Fail closed if status indicates not ready or required fields missing.
 */
bool ResolveSnapshotFromManifest(const std::string& manifest_json_or_url,
                                 SnapshotArtifactManifest& out,
                                 std::string& error,
                                 int timeout_sec = 60);

/** True if s looks like a JSON object (starts with '{') rather than a URL/path. */
bool LooksLikeJsonObject(const std::string& s);

/**
 * Pre-download Fast Sync gates (cheap; fail before multi-GB pull when possible):
 *  1) Manifest height attested in mapAssumeutxo (unless -assumeutxodev / regtest)
 *  2) Manifest hash_serialized matches chainparams for that height (when both present)
 *  3) Free disk on datadir volume >= size_bytes + margin (when size known)
 *  4) Optional HTTP(S) probe that artifact URL is reachable (HEAD/GET headers)
 *
 * Does not replace fail-closed file SHA-256 or post-load UTXO hash checks.
 * require_attested_height: true for product Fast Sync UI / default path.
 */
bool PreValidateSnapshotForFastSync(const SnapshotArtifactManifest& m,
                                    const std::string& artifact_url,
                                    std::string& error,
                                    bool require_attested_height = true);

/** Free-space check only (size_bytes from manifest; 0 skips). */
bool EnsureDiskSpaceForSnapshot(int64_t size_bytes, std::string& error);

/** Lightweight reachability probe for http(s) artifact URL (not a full download). */
bool ProbeHttpUrlReachable(const std::string& url, std::string& error, int timeout_sec = 30);

#endif // DOGECOIN_NODE_SNAPSHOT_FETCH_H
