// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node/snapshot_fetch.h"

#include "crypto/sha256.h"
#include "support/events.h"
#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <univalue.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>

namespace {

static const size_t STREAM_BUF = 1024 * 1024; // 1 MiB

bool IsHttpUrl(const std::string& s)
{
    return s.compare(0, 7, "http://") == 0 || s.compare(0, 8, "https://") == 0;
}

void HashToUint256(CSHA256& hasher, uint256& out)
{
    unsigned char buf[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(buf);
    memcpy(out.begin(), buf, 32);
}

bool StreamHashFromFILE(FILE* f, CSHA256& hasher, uint64_t& bytes_out, std::string& error)
{
    bytes_out = 0;
    std::vector<unsigned char> buf(STREAM_BUF);
    while (true) {
        size_t n = fread(buf.data(), 1, buf.size(), f);
        if (n > 0) {
            hasher.Write(buf.data(), n);
            bytes_out += n;
        }
        if (n < buf.size()) {
            if (feof(f))
                return true;
            if (ferror(f)) {
                error = "I/O error while reading stream";
                return false;
            }
        }
    }
}

struct HttpDownloadCtx {
    FILE* file;
    CSHA256* hasher;
    uint64_t bytes;
    std::string error;
    bool failed;
    bool done;
    int http_status;

    HttpDownloadCtx() : file(NULL), hasher(NULL), bytes(0), failed(false), done(false), http_status(0) {}
};

// libevent chunked callback: drain input buffer into file + hasher
static void SnapshotDownloadChunk(struct evhttp_request* req, void* arg)
{
    HttpDownloadCtx* ctx = static_cast<HttpDownloadCtx*>(arg);
    if (!ctx || ctx->failed || !ctx->file || !ctx->hasher)
        return;
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    if (!buf)
        return;
    while (evbuffer_get_length(buf) > 0) {
        size_t n = evbuffer_get_length(buf);
        if (n > STREAM_BUF)
            n = STREAM_BUF;
        unsigned char tmp[STREAM_BUF];
        // pull up to STREAM_BUF
        size_t got = evbuffer_remove(buf, tmp, n);
        if (got == 0)
            break;
        if (fwrite(tmp, 1, got, ctx->file) != got) {
            ctx->failed = true;
            ctx->error = "Failed writing download to disk";
            return;
        }
        ctx->hasher->Write(tmp, got);
        ctx->bytes += got;
    }
}

static void SnapshotDownloadDone(struct evhttp_request* req, void* arg)
{
    HttpDownloadCtx* ctx = static_cast<HttpDownloadCtx*>(arg);
    if (!ctx)
        return;
    ctx->done = true;
    if (!req) {
        ctx->failed = true;
        if (ctx->error.empty())
            ctx->error = "HTTP request failed (null response)";
        return;
    }
    ctx->http_status = evhttp_request_get_response_code(req);
    // Drain any remaining buffered body (non-chunked)
    SnapshotDownloadChunk(req, arg);
    if (ctx->http_status < 200 || ctx->http_status >= 300) {
        ctx->failed = true;
        ctx->error = strprintf("HTTP error status %d", ctx->http_status);
    }
}

bool ParseUrl(const std::string& url, std::string& host, int& port, std::string& path, bool& ssl, std::string& error)
{
    ssl = false;
    port = 80;
    std::string rest;
    if (url.compare(0, 8, "https://") == 0) {
        ssl = true;
        port = 443;
        rest = url.substr(8);
    } else if (url.compare(0, 7, "http://") == 0) {
        rest = url.substr(7);
    } else {
        error = "URL must start with http:// or https://";
        return false;
    }
    size_t slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    path = slash == std::string::npos ? "/" : rest.substr(slash);
    size_t colon = hostport.rfind(':');
    // IPv6 not fully handled; fine for CDN hostnames
    if (colon != std::string::npos && hostport.find(']') == std::string::npos) {
        host = hostport.substr(0, colon);
        port = atoi(hostport.substr(colon + 1).c_str());
        if (port <= 0) {
            error = "Invalid port in URL";
            return false;
        }
    } else {
        host = hostport;
    }
    if (host.empty()) {
        error = "Empty host in URL";
        return false;
    }
    return true;
}

} // namespace

bool ParseSha256Hex(const std::string& hex, uint256& out, std::string& error)
{
    std::string h = hex;
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X'))
        h = h.substr(2);
    if (h.size() != 64 || !IsHex(h)) {
        error = "artifact SHA-256 must be 64 hex characters";
        return false;
    }
    out.SetHex(h);
    return true;
}

bool HashFileSha256(const fs::path& path, uint256& hash_out, uint64_t& bytes_out, std::string& error)
{
    FILE* f = fsbridge::fopen(path, "rb");
    if (!f) {
        error = strprintf("Cannot open file for hashing: %s", path.string());
        return false;
    }
    CSHA256 hasher;
    bool ok = StreamHashFromFILE(f, hasher, bytes_out, error);
    fclose(f);
    if (!ok)
        return false;
    HashToUint256(hasher, hash_out);
    return true;
}

bool CopyFileStreamHash(const fs::path& source,
                        const fs::path& dest,
                        uint256& hash_out,
                        uint64_t& bytes_out,
                        std::string& error)
{
    FILE* in = fsbridge::fopen(source, "rb");
    if (!in) {
        error = strprintf("Cannot open source: %s", source.string());
        return false;
    }
    fs::path partial = dest;
    partial += ".partial";
    FILE* out = fsbridge::fopen(partial, "wb");
    if (!out) {
        fclose(in);
        error = strprintf("Cannot create partial dest: %s", partial.string());
        return false;
    }

    CSHA256 hasher;
    bytes_out = 0;
    std::vector<unsigned char> buf(STREAM_BUF);
    bool ok = true;
    while (ok) {
        size_t n = fread(buf.data(), 1, buf.size(), in);
        if (n > 0) {
            if (fwrite(buf.data(), 1, n, out) != n) {
                error = "Write failed during copy";
                ok = false;
                break;
            }
            hasher.Write(buf.data(), n);
            bytes_out += n;
        }
        if (n < buf.size()) {
            if (ferror(in)) {
                error = "Read failed during copy";
                ok = false;
            }
            break;
        }
    }
    fclose(in);
    if (fclose(out) != 0) {
        error = "Failed closing partial file";
        ok = false;
    }
    if (!ok) {
        boost::system::error_code ec;
        fs::remove(partial, ec);
        return false;
    }
    HashToUint256(hasher, hash_out);
    boost::system::error_code ec;
    fs::remove(dest, ec);
    fs::rename(partial, dest, ec);
    if (ec) {
        error = strprintf("Failed to rename partial to dest: %s", ec.message());
        fs::remove(partial, ec);
        return false;
    }
    return true;
}

bool DownloadUrlStreamHash(const std::string& url,
                           const fs::path& dest,
                           uint256& hash_out,
                           uint64_t& bytes_out,
                           std::string& error,
                           int timeout_sec)
{
    std::string host, path;
    int port = 80;
    bool ssl = false;
    if (!ParseUrl(url, host, port, path, ssl, error))
        return false;
    if (ssl) {
        // libevent without openssl wrapper in this tree: prefer http for now or fail clearly.
        // Many CDN URLs are https — try anyway via evhttp (may fail without bufferevent openssl).
        // Document limitation; operators can place file locally and use path fetch.
    }

    fs::path partial = dest;
    partial += ".partial";
    FILE* out = fsbridge::fopen(partial, "wb");
    if (!out) {
        error = strprintf("Cannot create partial download file: %s", partial.string());
        return false;
    }

    CSHA256 hasher;
    HttpDownloadCtx ctx;
    ctx.file = out;
    ctx.hasher = &hasher;

    try {
        raii_event_base base = obtain_event_base();
        raii_evhttp_connection evcon =
            obtain_evhttp_connection_base(base.get(), host, static_cast<uint16_t>(port));
        evhttp_connection_set_timeout(evcon.get(), timeout_sec);

        struct evhttp_request* req = evhttp_request_new(SnapshotDownloadDone, &ctx);
        if (!req) {
            fclose(out);
            error = "evhttp_request_new failed";
            return false;
        }
        evhttp_request_set_chunked_cb(req, SnapshotDownloadChunk);

        struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
        evhttp_add_header(headers, "Host", host.c_str());
        evhttp_add_header(headers, "Connection", "close");
        evhttp_add_header(headers, "User-Agent", "DogecoinCore-Pro-snapshot-fetch/1.14");

        if (evhttp_make_request(evcon.get(), req, EVHTTP_REQ_GET, path.c_str()) != 0) {
            fclose(out);
            error = "evhttp_make_request failed";
            boost::system::error_code ec;
            fs::remove(partial, ec);
            return false;
        }

        event_base_dispatch(base.get());
    } catch (const std::exception& e) {
        fclose(out);
        boost::system::error_code ec;
        fs::remove(partial, ec);
        error = e.what();
        return false;
    }
    fclose(out);

    if (!ctx.done || ctx.failed) {
        if (ctx.error.empty())
            ctx.error = "Download failed";
        error = ctx.error;
        if (ssl) {
            error += " (note: https may require a local file path if libevent SSL is unavailable)";
        }
        boost::system::error_code ec;
        fs::remove(partial, ec);
        return false;
    }

    bytes_out = ctx.bytes;
    HashToUint256(hasher, hash_out);

    boost::system::error_code ec;
    fs::remove(dest, ec);
    fs::rename(partial, dest, ec);
    if (ec) {
        error = strprintf("Failed to finalize download: %s", ec.message());
        fs::remove(partial, ec);
        return false;
    }
    return true;
}

bool FetchSnapshotArtifact(const std::string& source,
                           const fs::path& dest,
                           const uint256& expected_sha256,
                           uint64_t& bytes_out,
                           std::string& error,
                           int timeout_sec)
{
    uint256 got;
    bool ok = false;
    if (IsHttpUrl(source)) {
        ok = DownloadUrlStreamHash(source, dest, got, bytes_out, error, timeout_sec);
    } else {
        fs::path src(source);
        ok = CopyFileStreamHash(src, dest, got, bytes_out, error);
    }
    if (!ok)
        return false;

    if (got != expected_sha256) {
        boost::system::error_code ec;
        fs::remove(dest, ec);
        error = strprintf(
            "Artifact SHA-256 mismatch (fail closed): got %s expected %s — deleted %s",
            got.GetHex(), expected_sha256.GetHex(), dest.string());
        return false;
    }
    return true;
}

bool ParseSnapshotArtifactManifest(const std::string& json, SnapshotArtifactManifest& out, std::string& error)
{
    UniValue v;
    if (!v.read(json) || !v.isObject()) {
        error = "Manifest is not a JSON object";
        return false;
    }
    SnapshotArtifactManifest m;
    if (v.exists("height"))
        m.height = v["height"].get_int();
    if (v.exists("base_blockhash"))
        m.base_blockhash_hex = v["base_blockhash"].get_str();
    if (v.exists("hash_serialized"))
        m.hash_serialized_hex = v["hash_serialized"].get_str();
    if (v.exists("artifact_sha256"))
        m.artifact_sha256_hex = v["artifact_sha256"].get_str();
    else if (v.exists("sha256"))
        m.artifact_sha256_hex = v["sha256"].get_str();
    if (v.exists("url"))
        m.url = v["url"].get_str();
    if (v.exists("size_bytes"))
        m.size_bytes = v["size_bytes"].get_int64();

    if (m.artifact_sha256_hex.empty()) {
        error = "Manifest missing artifact_sha256 (or sha256)";
        return false;
    }
    uint256 discard;
    std::string herr;
    if (!ParseSha256Hex(m.artifact_sha256_hex, discard, herr)) {
        error = herr;
        return false;
    }
    out = m;
    return true;
}
