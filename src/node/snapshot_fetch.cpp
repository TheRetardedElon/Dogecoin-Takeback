// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node/snapshot_fetch.h"

#include "chainparams.h"
#include "crypto/sha256.h"
#include "support/events.h"
#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <univalue.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace {

static const size_t STREAM_BUF = 1024 * 1024; // 1 MiB

bool IsHttpUrl(const std::string& s)
{
    return s.compare(0, 7, "http://") == 0 || s.compare(0, 8, "https://") == 0;
}

void HashToUint256(CSHA256& hasher, uint256& out)
{
    // CSHA256 emits standard big-endian digest bytes.
    // uint256 GetHex/SetHex reverse for display (Bitcoin base_blob convention),
    // so store reversed so GetHex() matches sha256sum / user-supplied hex.
    unsigned char buf[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(buf);
    for (int i = 0; i < 32; i++) {
        out.begin()[i] = buf[31 - i];
    }
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
    SnapshotProgressFn progress;
    void* progress_ctx;
    uint64_t last_report;
    int64_t expected_bytes;

    HttpDownloadCtx()
        : file(NULL), hasher(NULL), bytes(0), failed(false), done(false), http_status(0),
          progress(0), progress_ctx(0), last_report(0), expected_bytes(-1) {}
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
        if (ctx->progress && (ctx->bytes - ctx->last_report >= STREAM_BUF || ctx->bytes < STREAM_BUF)) {
            ctx->last_report = ctx->bytes;
            if (!ctx->progress(ctx->bytes, ctx->expected_bytes, ctx->progress_ctx)) {
                ctx->failed = true;
                ctx->error = "Download aborted by user";
                return;
            }
        }
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

#ifdef WIN32
/** Windows HTTPS/HTTP via WinHTTP (Schannel) — required for CDN Fast Sync on PE builds. */
static bool DownloadUrlStreamHashWinHttp(const std::string& url,
                                         const fs::path& dest,
                                         uint256& hash_out,
                                         uint64_t& bytes_out,
                                         std::string& error,
                                         int timeout_sec,
                                         SnapshotProgressFn progress,
                                         void* progress_ctx)
{
    std::string host, path;
    int port = 80;
    bool ssl = false;
    if (!ParseUrl(url, host, port, path, ssl, error))
        return false;

    auto to_wide = [](const std::string& s) -> std::wstring {
        if (s.empty())
            return std::wstring();
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        if (n <= 0)
            return std::wstring();
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
        if (!w.empty() && w.back() == L'\0')
            w.pop_back();
        return w;
    };
    std::wstring whost = to_wide(host);
    std::wstring wpath = to_wide(path);
    if (whost.empty() || wpath.empty()) {
        error = "Failed to convert URL to UTF-16 for WinHTTP";
        return false;
    }

    fs::path partial = dest;
    partial += ".partial";
    FILE* out = fsbridge::fopen(partial, "wb");
    if (!out) {
        error = strprintf("Cannot create partial download file: %s", partial.string());
        return false;
    }

    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    bool ok = false;
    CSHA256 hasher;
    int64_t expected_bytes = -1;
    bytes_out = 0;

    hSession = WinHttpOpen(L"DogecoinCore-Pro-snapshot-fetch/1.14",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS,
                           0);
    if (!hSession) {
        error = strprintf("WinHttpOpen failed (%lu)", GetLastError());
        fclose(out);
        boost::system::error_code ec;
        fs::remove(partial, ec);
        return false;
    }

    int to_ms = timeout_sec > 0 ? timeout_sec * 1000 : 600000;
    WinHttpSetTimeouts(hSession, to_ms, to_ms, to_ms, to_ms);

    hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!hConnect) {
        error = strprintf("WinHttpConnect failed (%lu)", GetLastError());
        goto winhttp_cleanup;
    }

    {
        DWORD flags = ssl ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                                      NULL, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    }
    if (!hRequest) {
        error = strprintf("WinHttpOpenRequest failed (%lu)", GetLastError());
        goto winhttp_cleanup;
    }

    {
        DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = strprintf("WinHttpSendRequest failed (%lu)", GetLastError());
        goto winhttp_cleanup;
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        error = strprintf("WinHttpReceiveResponse failed (%lu)", GetLastError());
        goto winhttp_cleanup;
    }

    {
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(hRequest,
                                 WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                 WINHTTP_NO_HEADER_INDEX)) {
            error = strprintf("WinHttpQueryHeaders status failed (%lu)", GetLastError());
            goto winhttp_cleanup;
        }
        if (status < 200 || status >= 300) {
            error = strprintf("HTTP error status %lu (WinHTTP)", status);
            goto winhttp_cleanup;
        }
    }

    {
        wchar_t clen_buf[64];
        DWORD clen_size = sizeof(clen_buf);
        if (WinHttpQueryHeaders(hRequest,
                                WINHTTP_QUERY_CONTENT_LENGTH,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                clen_buf, &clen_size,
                                WINHTTP_NO_HEADER_INDEX)) {
            // Content-Length as wide decimal string
            std::string clen;
            for (wchar_t* p = clen_buf; *p; ++p) {
                if (*p >= L'0' && *p <= L'9')
                    clen.push_back(static_cast<char>(*p));
            }
            if (!clen.empty()) {
                expected_bytes = static_cast<int64_t>(strtoull(clen.c_str(), NULL, 10));
            }
        }
    }

    {
        std::vector<unsigned char> buf(STREAM_BUF);
        uint64_t last_report = 0;
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
                error = strprintf("WinHttpQueryDataAvailable failed (%lu)", GetLastError());
                goto winhttp_cleanup;
            }
            if (avail == 0)
                break;
            DWORD to_read = avail;
            if (to_read > STREAM_BUF)
                to_read = static_cast<DWORD>(STREAM_BUF);
            DWORD got = 0;
            if (!WinHttpReadData(hRequest, buf.data(), to_read, &got)) {
                error = strprintf("WinHttpReadData failed (%lu)", GetLastError());
                goto winhttp_cleanup;
            }
            if (got == 0)
                break;
            if (fwrite(buf.data(), 1, got, out) != got) {
                error = "Failed writing WinHTTP download to disk";
                goto winhttp_cleanup;
            }
            hasher.Write(buf.data(), got);
            bytes_out += got;
            // Report every ~1 MiB (or always if small) so GUI progress stays live
            if (progress && (bytes_out - last_report >= STREAM_BUF || bytes_out < STREAM_BUF)) {
                last_report = bytes_out;
                if (!progress(bytes_out, expected_bytes, progress_ctx)) {
                    error = "Download aborted by user";
                    goto winhttp_cleanup;
                }
            }
        }
        if (progress) {
            if (!progress(bytes_out, expected_bytes, progress_ctx)) {
                error = "Download aborted by user";
                goto winhttp_cleanup;
            }
        }
    }

    ok = true;

winhttp_cleanup:
    if (hRequest)
        WinHttpCloseHandle(hRequest);
    if (hConnect)
        WinHttpCloseHandle(hConnect);
    if (hSession)
        WinHttpCloseHandle(hSession);

    if (fclose(out) != 0 && ok) {
        error = "Failed closing partial download file";
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
        error = strprintf("Failed to finalize download: %s", ec.message());
        fs::remove(partial, ec);
        return false;
    }
    return true;
}
#endif // WIN32

bool DownloadUrlStreamHash(const std::string& url,
                           const fs::path& dest,
                           uint256& hash_out,
                           uint64_t& bytes_out,
                           std::string& error,
                           int timeout_sec,
                           SnapshotProgressFn progress,
                           void* progress_ctx)
{
    std::string host, path;
    int port = 80;
    bool ssl = false;
    if (!ParseUrl(url, host, port, path, ssl, error))
        return false;

#ifdef WIN32
    // Prefer WinHTTP (Schannel) for http(s) on Windows — libevent has no SSL here.
    return DownloadUrlStreamHashWinHttp(url, dest, hash_out, bytes_out, error, timeout_sec,
                                        progress, progress_ctx);
#else
    (void)host;
    (void)port;
    // libevent path (typically plain HTTP only unless built with SSL)
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
    ctx.progress = progress;
    ctx.progress_ctx = progress_ctx;
    ctx.last_report = 0;

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
#endif
}

bool FetchSnapshotArtifact(const std::string& source,
                           const fs::path& dest,
                           const uint256& expected_sha256,
                           uint64_t& bytes_out,
                           std::string& error,
                           int timeout_sec,
                           SnapshotProgressFn progress,
                           void* progress_ctx)
{
    uint256 got;
    bool ok = false;
    if (IsHttpUrl(source)) {
        ok = DownloadUrlStreamHash(source, dest, got, bytes_out, error, timeout_sec,
                                   progress, progress_ctx);
    } else {
        fs::path src(source);
        ok = CopyFileStreamHash(src, dest, got, bytes_out, error);
        if (ok && progress)
            progress(bytes_out, static_cast<int64_t>(bytes_out), progress_ctx);
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

bool FetchSnapshotArtifactFromCandidates(const std::vector<std::string>& sources,
                                         const fs::path& dest,
                                         const uint256& expected_sha256,
                                         uint64_t& bytes_out,
                                         std::string& error,
                                         std::string* used_source_out,
                                         int timeout_sec,
                                         SnapshotProgressFn progress,
                                         void* progress_ctx)
{
    if (sources.empty()) {
        error = "No snapshot source URLs/paths to try";
        return false;
    }
    std::string combined;
    for (size_t i = 0; i < sources.size(); ++i) {
        const std::string& src = sources[i];
        if (src.empty())
            continue;
        std::string attempt_err;
        uint64_t attempt_bytes = 0;
        // Clean dest between attempts so a partial from host A is not kept on fail
        {
            boost::system::error_code ec;
            fs::remove(dest, ec);
            fs::path partial = dest;
            partial += ".partial";
            fs::remove(partial, ec);
        }
        if (FetchSnapshotArtifact(src, dest, expected_sha256, attempt_bytes, attempt_err,
                                  timeout_sec, progress, progress_ctx)) {
            bytes_out = attempt_bytes;
            if (used_source_out)
                *used_source_out = src;
            error.clear();
            return true;
        }
        if (!combined.empty())
            combined += " | ";
        combined += strprintf("[%u] %s: %s", static_cast<unsigned>(i + 1), src, attempt_err);
    }
    error = strprintf("All snapshot sources failed (fail closed): %s", combined);
    return false;
}

bool LooksLikeJsonObject(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n'))
        ++i;
    return i < s.size() && s[i] == '{';
}

static bool SourceIsHttpUrl(const std::string& s)
{
    return s.compare(0, 7, "http://") == 0 || s.compare(0, 8, "https://") == 0;
}

bool ParseSnapshotArtifactManifest(const std::string& json, SnapshotArtifactManifest& out, std::string& error)
{
    UniValue v;
    if (!v.read(json) || !v.isObject()) {
        error = "Manifest is not a JSON object";
        return false;
    }
    SnapshotArtifactManifest m;
    if (v.exists("status") && v["status"].isStr())
        m.status = v["status"].get_str();
    // height / blocks (GPE latest.json uses blocks)
    if (v.exists("height"))
        m.height = v["height"].get_int();
    else if (v.exists("blocks"))
        m.height = v["blocks"].get_int();
    // base block hash aliases
    if (v.exists("base_blockhash"))
        m.base_blockhash_hex = v["base_blockhash"].get_str();
    else if (v.exists("bestblock"))
        m.base_blockhash_hex = v["bestblock"].get_str();
    if (v.exists("hash_serialized"))
        m.hash_serialized_hex = v["hash_serialized"].get_str();
    if (v.exists("artifact_sha256"))
        m.artifact_sha256_hex = v["artifact_sha256"].get_str();
    else if (v.exists("sha256"))
        m.artifact_sha256_hex = v["sha256"].get_str();
    if (v.exists("url"))
        m.url = v["url"].get_str();
    // mesh M2: urls[] and/or mirrors[].url — same artifact digest for every host
    if (v.exists("urls") && v["urls"].isArray()) {
        const UniValue& arr = v["urls"];
        for (size_t i = 0; i < arr.size(); ++i) {
            if (arr[i].isStr()) {
                const std::string u = arr[i].get_str();
                if (!u.empty())
                    m.urls.push_back(u);
            }
        }
    }
    if (v.exists("mirrors") && v["mirrors"].isArray()) {
        const UniValue& arr = v["mirrors"];
        for (size_t i = 0; i < arr.size(); ++i) {
            if (!arr[i].isObject())
                continue;
            if (arr[i].exists("url") && arr[i]["url"].isStr()) {
                const std::string u = arr[i]["url"].get_str();
                if (!u.empty())
                    m.urls.push_back(u);
            }
        }
    }
    // size aliases
    if (v.exists("size_bytes"))
        m.size_bytes = v["size_bytes"].get_int64();
    else if (v.exists("bytes"))
        m.size_bytes = v["bytes"].get_int64();

    // Placeholder CDN page before first dump
    if (!m.status.empty() && m.artifact_sha256_hex.empty()) {
        error = strprintf(
            "Snapshot not published yet (status=%s). Wait for gpednode dumptxoutset / CDN pull.",
            m.status);
        return false;
    }

    if (m.artifact_sha256_hex.empty()) {
        error = "Manifest missing artifact_sha256 (or sha256)";
        return false;
    }
    // url optional if urls[] non-empty
    if (m.url.empty() && !m.urls.empty())
        m.url = m.urls.front();
    if (m.url.empty()) {
        error = "Manifest missing url (or urls[]) for .dat artifact";
        return false;
    }
    // Ensure primary is first candidate
    if (m.urls.empty())
        m.urls.push_back(m.url);
    else {
        bool has_primary = false;
        for (size_t i = 0; i < m.urls.size(); ++i) {
            if (m.urls[i] == m.url) {
                has_primary = true;
                break;
            }
        }
        if (!has_primary)
            m.urls.insert(m.urls.begin(), m.url);
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

std::vector<std::string> SnapshotArtifactManifest::CandidateUrls() const
{
    std::vector<std::string> out;
    std::vector<std::string> seen;
    auto add = [&](const std::string& u) {
        if (u.empty())
            return;
        for (size_t i = 0; i < seen.size(); ++i) {
            if (seen[i] == u)
                return;
        }
        seen.push_back(u);
        out.push_back(u);
    };
    add(url);
    for (size_t i = 0; i < urls.size(); ++i)
        add(urls[i]);
    return out;
}

bool EnsureDiskSpaceForSnapshot(int64_t size_bytes, std::string& error)
{
    if (size_bytes <= 0)
        return true;
    // Snapshot file + room to unpack into chainstate_snapshot (margin).
    const int64_t margin = 2LL * 1024 * 1024 * 1024; // 2 GiB
    const uintmax_t need = static_cast<uintmax_t>(size_bytes) + static_cast<uintmax_t>(margin);

    boost::system::error_code ec;
    fs::path dir = GetDataDir();
    fs::space_info si = fs::space(dir, ec);
    if (ec) {
        si = fs::space(dir.parent_path(), ec);
    }
    if (ec) {
        LogPrintf("Fast Sync preflight: could not query free disk (%s); continuing\n",
                  ec.message());
        return true;
    }
    if (si.available < need) {
        const double need_gib = need / (1024.0 * 1024.0 * 1024.0);
        const double have_gib = si.available / (1024.0 * 1024.0 * 1024.0);
        const double snap_gib = size_bytes / (1024.0 * 1024.0 * 1024.0);
        error = strprintf(
            "Not enough free disk for Fast Sync: need ~%.1f GiB free "
            "(snapshot ~%.1f GiB + margin), have ~%.1f GiB on datadir volume",
            need_gib, snap_gib, have_gib);
        return false;
    }
    return true;
}

#ifdef WIN32
static bool ProbeHttpUrlReachableWinHttp(const std::string& url, std::string& error, int timeout_sec)
{
    std::string host, path;
    int port = 80;
    bool ssl = false;
    if (!ParseUrl(url, host, port, path, ssl, error))
        return false;

    auto to_wide = [](const std::string& s) -> std::wstring {
        if (s.empty())
            return std::wstring();
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        if (n <= 0)
            return std::wstring();
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
        if (!w.empty() && w.back() == L'\0')
            w.pop_back();
        return w;
    };
    std::wstring whost = to_wide(host);
    std::wstring wpath = to_wide(path);
    if (whost.empty() || wpath.empty()) {
        error = "Probe: failed to convert URL for WinHTTP";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"DogecoinCore-FastSyncProbe/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        error = "Probe: WinHttpOpen failed";
        return false;
    }
    const int timeout_ms = timeout_sec > 0 ? timeout_sec * 1000 : 30000;
    WinHttpSetTimeouts(hSession, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        error = "Probe: WinHttpConnect failed (host unreachable?)";
        return false;
    }

    DWORD flags = ssl ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", wpath.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        // Some CDNs dislike HEAD — fall back to Range GET of 1 byte
        hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                                      NULL, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (hRequest) {
            WinHttpAddRequestHeaders(hRequest, L"Range: bytes=0-0", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        }
    }
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        error = "Probe: WinHttpOpenRequest failed";
        return false;
    }

    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok)
        ok = WinHttpReceiveResponse(hRequest, NULL);
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (ok) {
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                            WINHTTP_NO_HEADER_INDEX);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!ok) {
        error = "Probe: no HTTP response from snapshot host (firewall / DNS / offline?)";
        return false;
    }
    // 2xx, 3xx redirect already followed by WinHTTP defaults for some cases; 206 Partial OK
    if (status >= 400) {
        error = strprintf("Probe: snapshot URL returned HTTP %lu", static_cast<unsigned long>(status));
        return false;
    }
    return true;
}
#endif

bool ProbeHttpUrlReachable(const std::string& url, std::string& error, int timeout_sec)
{
    if (!IsHttpUrl(url) && !SourceIsHttpUrl(url)) {
        // Local path
        if (!fs::exists(url)) {
            error = strprintf("Snapshot path does not exist: %s", url);
            return false;
        }
        return true;
    }
#ifdef WIN32
    return ProbeHttpUrlReachableWinHttp(url, error, timeout_sec);
#else
    // Non-Windows: full download of multi-GB is too heavy; skip deep probe
    // after manifest already fetched from CDN. Still check URL shape.
    (void)timeout_sec;
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        error = "Probe: expected http(s) URL or local path";
        return false;
    }
    LogPrintf("Fast Sync preflight: skipping deep HTTP probe on non-Windows for %s\n", url);
    return true;
#endif
}

bool PreValidateSnapshotForFastSync(const SnapshotArtifactManifest& m,
                                    const std::string& artifact_url,
                                    std::string& error,
                                    bool require_attested_height)
{
    const bool allow_unattested =
        GetBoolArg("-assumeutxodev", false) ||
        Params().NetworkIDString() == "regtest";

    // --- height + mapAssumeutxo + hash_serialized (before multi-GB download) ---
    if (m.height >= 0) {
        const AssumeutxoData* att = Params().AssumeutxoForHeight(m.height);
        if (!att) {
            if (require_attested_height && !allow_unattested) {
                error = strprintf(
                    "Snapshot height %d is not attested in this Dogecoin Core Pro build "
                    "(mapAssumeutxo). Install a client that includes this height, or wait "
                    "for a matching release. Advanced only: -assumeutxodev=1 (not for real funds).",
                    m.height);
                return false;
            }
        } else if (!m.hash_serialized_hex.empty()) {
            uint256 manifest_hs;
            std::string herr;
            // Accept with or without 0x
            std::string hex = m.hash_serialized_hex;
            if (hex.size() >= 2 && (hex[0] == '0') && (hex[1] == 'x' || hex[1] == 'X'))
                hex = hex.substr(2);
            if (!ParseSha256Hex(hex, manifest_hs, herr)) {
                // hash_serialized uses same hex encoding as GetHex
                try {
                    manifest_hs = uint256S(hex);
                } catch (...) {
                    error = strprintf("Manifest hash_serialized is not valid hex: %s", herr);
                    return false;
                }
            }
            if (manifest_hs != att->hash_serialized) {
                error = strprintf(
                    "Manifest hash_serialized does not match this build's mapAssumeutxo[%d] "
                    "(manifest=%s expected=%s). CDN dump and client attestation are out of sync — "
                    "do not download; update client or republish a matching dump.",
                    m.height, manifest_hs.GetHex(), att->hash_serialized.GetHex());
                return false;
            }
        }
    } else if (require_attested_height && !allow_unattested &&
               artifact_url.find("latest.json") == std::string::npos) {
        // Direct custom .dat without height: cannot pre-check mapAssumeutxo; still allow
        // download (SHA fail-closed) but log.
        LogPrintf("Fast Sync preflight: no height in manifest; skipping mapAssumeutxo pre-check\n");
    }

    if (!EnsureDiskSpaceForSnapshot(m.size_bytes, error))
        return false;

    std::string probe_url = artifact_url;
    if (probe_url.empty())
        probe_url = m.url;
    if (!probe_url.empty() && (IsHttpUrl(probe_url) || SourceIsHttpUrl(probe_url))) {
        std::string perr;
        if (!ProbeHttpUrlReachable(probe_url, perr, 30)) {
            error = perr;
            return false;
        }
    }

    return true;
}

bool DownloadUrlToString(const std::string& url, std::string& body_out, std::string& error, int timeout_sec)
{
    // Reuse stream-hash path into a temp file under datadir snapshots (or system temp).
    fs::path tmp;
    try {
        tmp = GetDataDir() / "snapshots" / "manifest_fetch.json";
    } catch (...) {
        tmp = fs::temp_directory_path() / "dogecoin_manifest_fetch.json";
    }
    {
        boost::system::error_code ec;
        fs::create_directories(tmp.parent_path(), ec);
    }
    uint256 hash_discard;
    uint64_t bytes = 0;
    if (!DownloadUrlStreamHash(url, tmp, hash_discard, bytes, error, timeout_sec))
        return false;

    FILE* f = fsbridge::fopen(tmp, "rb");
    if (!f) {
        error = strprintf("Cannot read downloaded manifest: %s", tmp.string());
        boost::system::error_code ec;
        fs::remove(tmp, ec);
        return false;
    }
    body_out.clear();
    body_out.reserve(static_cast<size_t>(bytes > 0 && bytes < (1 << 20) ? bytes : 4096));
    char buf[4096];
    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n > 0)
            body_out.append(buf, n);
        if (n < sizeof(buf))
            break;
        if (body_out.size() > 2 * 1024 * 1024) {
            fclose(f);
            error = "Manifest body too large (>2 MiB)";
            boost::system::error_code ec;
            fs::remove(tmp, ec);
            return false;
        }
    }
    fclose(f);
    boost::system::error_code ec;
    fs::remove(tmp, ec);
    return true;
}

bool ResolveSnapshotFromManifest(const std::string& manifest_json_or_url,
                                 SnapshotArtifactManifest& out,
                                 std::string& error,
                                 int timeout_sec)
{
    std::string json = manifest_json_or_url;
    if (!LooksLikeJsonObject(manifest_json_or_url)) {
        if (!SourceIsHttpUrl(manifest_json_or_url)) {
            // local file path to JSON
            fs::path p(manifest_json_or_url);
            FILE* f = fsbridge::fopen(p, "rb");
            if (!f) {
                error = strprintf("Cannot open manifest file: %s", p.string());
                return false;
            }
            json.clear();
            char buf[4096];
            while (true) {
                size_t n = fread(buf, 1, sizeof(buf), f);
                if (n > 0)
                    json.append(buf, n);
                if (n < sizeof(buf))
                    break;
            }
            fclose(f);
        } else if (!DownloadUrlToString(manifest_json_or_url, json, error, timeout_sec)) {
            return false;
        }
    }
    return ParseSnapshotArtifactManifest(json, out, error);
}
