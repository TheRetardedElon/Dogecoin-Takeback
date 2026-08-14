#include "rpc_client.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
static bool EnsureWsa()
{
    static bool once = false, ok = false;
    if (!once) {
        WSADATA w;
        ok = (WSAStartup(MAKEWORD(2, 2), &w) == 0);
        once = true;
    }
    return ok;
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using SOCKET = int;
static const int INVALID_SOCKET = -1;
static const int SOCKET_ERROR = -1;
static void closesocket(int s) { ::close(s); }
static bool EnsureWsa() { return true; }
#endif

static bool SetNonBlocking(SOCKET s, bool nb)
{
#if defined(_WIN32)
    u_long mode = nb ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    if (nb) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(s, F_SETFL, flags) == 0;
#endif
}

static std::string Base64(const std::string& in)
{
    static const char* t =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(t[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(t[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string ReadFileLine(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string s;
    std::getline(f, s);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

static std::vector<std::string> DefaultCookiePaths()
{
    std::vector<std::string> v;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        v.push_back(std::string(appdata) + "\\Dogecoin\\.cookie");
        v.push_back(std::string(appdata) + "\\Dogecoin\\testnet3\\.cookie");
        v.push_back(std::string(appdata) + "\\Dogecoin\\regtest\\.cookie");
    }
#else
    if (const char* home = std::getenv("HOME")) {
        v.push_back(std::string(home) + "/.dogecoin/.cookie");
        v.push_back(std::string(home) + "/.dogecoin/testnet3/.cookie");
        v.push_back(std::string(home) + "/.dogecoin/regtest/.cookie");
    }
#endif
    return v;
}

RpcClient::RpcClient(RpcConfig cfg) : m_cfg(std::move(cfg)) {}

bool RpcClient::portOpen(int timeoutMs) const
{
    if (!EnsureWsa()) return false;
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%d", m_cfg.port);
    struct addrinfo hints {};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    struct addrinfo* res = nullptr;
    if (getaddrinfo(m_cfg.host.c_str(), portStr, &hints, &res) != 0 || !res)
        return false;
    bool ok = false;
    for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {
        SOCKET s = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        SetNonBlocking(s, true);
        int cr = ::connect(s, ai->ai_addr, (int)ai->ai_addrlen);
#if defined(_WIN32)
        const bool inProgress = (cr == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
#else
        const bool inProgress = (cr < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK));
#endif
        if (cr == 0) {
            ok = true;
            closesocket(s);
            break;
        }
        if (inProgress) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(s, &wfds);
            struct timeval tv;
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            if (select((int)s + 1, nullptr, &wfds, nullptr, &tv) > 0) {
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
#if defined(_WIN32)
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl);
#else
                getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &sl);
#endif
                if (soerr == 0) ok = true;
            }
            closesocket(s);
            if (ok) break;
        } else {
            closesocket(s);
        }
    }
    freeaddrinfo(res);
    return ok;
}

std::string RpcClient::resolveAuthHeader() const
{
    // Cookie first (rotates each start). Fall back to dogecoin.conf user/pass.
    // Same-user malware can read either; cookie beats a copied old ini/conf.
    std::string cookie;
    if (!m_cfg.cookiePath.empty())
        cookie = ReadFileLine(m_cfg.cookiePath);
    if (cookie.empty()) {
        for (const auto& p : DefaultCookiePaths()) {
            cookie = ReadFileLine(p);
            if (!cookie.empty()) break;
        }
    }
    if (!cookie.empty())
        return "Basic " + Base64(cookie);
    if (!m_cfg.user.empty() && !m_cfg.password.empty())
        return "Basic " + Base64(m_cfg.user + ":" + m_cfg.password);
    return {};
}

RpcResult RpcClient::call(const std::string& method, const std::string& paramsJson) const
{
    RpcResult r;
    if (!EnsureWsa()) {
        r.error = "socket init failed";
        return r;
    }
    const std::string auth = resolveAuthHeader();
    if (auth.empty()) {
        r.error = "no RPC cookie/credentials (start dogecoind or set user/pass)";
        return r;
    }

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%d", m_cfg.port);
    struct addrinfo hints {};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    struct addrinfo* res = nullptr;
    if (getaddrinfo(m_cfg.host.c_str(), portStr, &hints, &res) != 0 || !res) {
        r.error = "resolve failed";
        return r;
    }
    SOCKET s = INVALID_SOCKET;
    for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {
        s = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        SetNonBlocking(s, true);
        int cr = ::connect(s, ai->ai_addr, (int)ai->ai_addrlen);
#if defined(_WIN32)
        const bool inProgress = (cr == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
#else
        const bool inProgress = (cr < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK));
#endif
        bool ready = (cr == 0);
        if (!ready && inProgress) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(s, &wfds);
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 400 * 1000;
            if (select((int)s + 1, nullptr, &wfds, nullptr, &tv) > 0) {
                int soerr = 0;
                socklen_t sl = sizeof(soerr);
#if defined(_WIN32)
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl);
#else
                getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &sl);
#endif
                ready = (soerr == 0);
            }
        }
        if (ready) {
            SetNonBlocking(s, false);
#if defined(_WIN32)
            DWORD rcv = 1500;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rcv, sizeof(rcv));
#else
            struct timeval rcv;
            rcv.tv_sec = 1;
            rcv.tv_usec = 500000;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rcv, sizeof(rcv));
#endif
            break;
        }
        closesocket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (s == INVALID_SOCKET) {
        r.error = "connect failed";
        return r;
    }

    std::ostringstream body;
    body << "{\"jsonrpc\":\"1.0\",\"id\":\"pro-gui\",\"method\":\"" << method
         << "\",\"params\":" << paramsJson << "}";
    const std::string bodyStr = body.str();

    std::ostringstream req;
    req << "POST / HTTP/1.1\r\n"
        << "Host: " << m_cfg.host << "\r\n"
        << "Authorization: " << auth << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << bodyStr.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << bodyStr;
    const std::string reqStr = req.str();
    if (::send(s, reqStr.data(), (int)reqStr.size(), 0) < 0) {
        r.error = "send failed";
        closesocket(s);
        return r;
    }

    std::string raw;
    char buf[8192];
    for (;;) {
#if defined(_WIN32)
        int n = ::recv(s, buf, sizeof(buf), 0);
#else
        ssize_t n = ::recv(s, buf, sizeof(buf), 0);
#endif
        if (n <= 0) break;
        raw.append(buf, buf + n);
        if (raw.size() > 2 * 1024 * 1024) break;
    }
    closesocket(s);

    // Split headers / body
    auto hdrEnd = raw.find("\r\n\r\n");
    std::string headers = hdrEnd == std::string::npos ? std::string() : raw.substr(0, hdrEnd);
    r.body = hdrEnd == std::string::npos ? raw : raw.substr(hdrEnd + 4);
    if (headers.size() >= 12 && headers.compare(0, 5, "HTTP/") == 0) {
        r.httpCode = std::atoi(headers.c_str() + 9);
    }

    if (r.body.find("Loading") != std::string::npos || r.body.find("warmup") != std::string::npos ||
        r.body.find("-28") != std::string::npos) {
        r.error = "node still warming up (AppInit)";
        return r;
    }

    // Extract "result": ...
    auto rp = r.body.find("\"result\"");
    if (rp == std::string::npos) {
        r.error = "no RPC result (auth/warmup/method?)";
        return r;
    }
    auto colon = r.body.find(':', rp);
    if (colon == std::string::npos) {
        r.error = "malformed result";
        return r;
    }
    size_t i = colon + 1;
    while (i < r.body.size() && std::isspace((unsigned char)r.body[i])) i++;
    if (i >= r.body.size()) {
        r.error = "empty result";
        return r;
    }
    // naive extract until matching top-level — for objects/arrays use brace depth
    if (r.body[i] == '{' || r.body[i] == '[') {
        char open = r.body[i], close = (open == '{') ? '}' : ']';
        int depth = 0;
        size_t j = i;
        for (; j < r.body.size(); ++j) {
            char c = r.body[j];
            if (c == open) depth++;
            else if (c == close) {
                depth--;
                if (depth == 0) {
                    j++;
                    break;
                }
            }
        }
        r.resultJson = r.body.substr(i, j - i);
    } else if (r.body[i] == '"') {
        size_t j = i + 1;
        while (j < r.body.size() && r.body[j] != '"') {
            if (r.body[j] == '\\' && j + 1 < r.body.size()) j += 2;
            else j++;
        }
        if (j < r.body.size()) j++;
        r.resultJson = r.body.substr(i, j - i);
    } else if (r.body.compare(i, 4, "null") == 0) {
        r.resultJson = "null";
    } else {
        size_t j = i;
        while (j < r.body.size() && r.body[j] != ',' && r.body[j] != '}') j++;
        r.resultJson = r.body.substr(i, j - i);
    }

    // error field
    auto ep = r.body.find("\"error\"");
    if (ep != std::string::npos) {
        auto sub = r.body.substr(ep, 48);
        if (sub.find("null") == std::string::npos) {
            r.error = "RPC error object present";
            // still may have partial result
            if (r.resultJson == "null" || r.resultJson.empty())
                return r;
        }
    }

    r.ok = true;
    return r;
}

std::string RpcClient::jsonString(const std::string& json, const char* key)
{
    std::string k = std::string("\"") + key + "\":";
    auto p = json.find(k);
    if (p == std::string::npos) return {};
    p += k.size();
    while (p < json.size() && std::isspace((unsigned char)json[p])) p++;
    if (p >= json.size()) return {};
    if (json[p] == '"') {
        auto e = p + 1;
        while (e < json.size() && json[e] != '"') {
            if (json[e] == '\\' && e + 1 < json.size()) e += 2;
            else e++;
        }
        if (e < json.size()) return json.substr(p + 1, e - p - 1);
        return {};
    }
    auto e = p;
    while (e < json.size() && json[e] != ',' && json[e] != '}' && json[e] != ']') e++;
    std::string v = json.substr(p, e - p);
    while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
    return v;
}

int RpcClient::jsonInt(const std::string& json, const char* key, int def)
{
    std::string v = jsonString(json, key);
    if (v.empty() || v == "null") return def;
    return std::atoi(v.c_str());
}

double RpcClient::jsonDouble(const std::string& json, const char* key, double def)
{
    std::string v = jsonString(json, key);
    if (v.empty() || v == "null") return def;
    return std::atof(v.c_str());
}

bool RpcClient::jsonBool(const std::string& json, const char* key, bool def)
{
    std::string v = jsonString(json, key);
    if (v == "true") return true;
    if (v == "false") return false;
    return def;
}

NodeSnapshot RpcClient::refreshSnapshot() const
{
    NodeSnapshot s;
    if (!portOpen(120)) {
        s.status = "Waiting for dogecoind RPC port...";
        return s;
    }

    auto chain = call("getblockchaininfo");
    if (!chain.ok) {
        s.status = chain.error.empty() ? "RPC not ready" : chain.error;
        s.rpcWarmup = (chain.error.find("warm") != std::string::npos ||
                       chain.error.find("Loading") != std::string::npos ||
                       chain.error.find("AppInit") != std::string::npos);
        // port open counts as partial
        s.connected = false;
        return s;
    }

    s.connected = true;
    s.chain = jsonString(chain.resultJson, "chain");
    s.blocks = jsonInt(chain.resultJson, "blocks");
    s.headers = jsonInt(chain.resultJson, "headers");
    s.verificationProgress = jsonDouble(chain.resultJson, "verificationprogress");
    s.initialBlockDownload = jsonBool(chain.resultJson, "initialblockdownload");
    s.bestBlockHash = jsonString(chain.resultJson, "bestblockhash");
    s.dbEngine = jsonString(chain.resultJson, "dbengine");
    s.status = "chain=" + (s.chain.empty() ? "?" : s.chain) +
               " blocks=" + std::to_string(s.blocks) +
               " headers=" + std::to_string(s.headers);

    auto net = call("getnetworkinfo");
    if (net.ok) {
        s.connections = jsonInt(net.resultJson, "connections");
        s.version = jsonInt(net.resultJson, "version");
        s.subversion = jsonString(net.resultJson, "subversion");
        s.networkActive = jsonString(net.resultJson, "networkactive");
        const std::string& nj = net.resultJson;
        auto px = nj.find("\"proxy\":");
        if (px != std::string::npos) {
            auto q1 = nj.find('"', px + 8);
            if (q1 != std::string::npos) {
                auto q2 = nj.find('"', q1 + 1);
                if (q2 != std::string::npos)
                    s.p2pProxy = nj.substr(q1 + 1, q2 - q1 - 1);
            }
        }
        auto on = nj.find(".onion");
        if (on != std::string::npos) {
            size_t a = on;
            while (a > 0 && nj[a - 1] != '"' && nj[a - 1] != ':')
                --a;
            s.onionAddress = nj.substr(a, (on + 6) - a);
        }
    }

    auto wal = call("getwalletinfo");
    if (wal.ok && wal.resultJson != "null" && !wal.resultJson.empty()) {
        s.hasWallet = true;
        s.balance = jsonDouble(wal.resultJson, "balance");
        s.unconfirmed = jsonDouble(wal.resultJson, "unconfirmed_balance");
        s.walletVersion = jsonInt(wal.resultJson, "walletversion");
        s.walletName = jsonString(wal.resultJson, "walletname");
        if (wal.resultJson.find("unlocked_until") != std::string::npos) {
            s.walletEncrypted = true;
            s.walletLocked = jsonInt(wal.resultJson, "unlocked_until") <= 0;
        }
    }

    auto peers = call("getpeerinfo");
    if (peers.ok && peers.resultJson.size() > 2 && peers.resultJson[0] == '[') {
        const std::string& j = peers.resultJson;
        // Split peer objects by "{...}" top-level in array
        size_t i = 0;
        while (i < j.size()) {
            auto o = j.find('{', i);
            if (o == std::string::npos) break;
            int depth = 0;
            size_t k = o;
            for (; k < j.size(); ++k) {
                if (j[k] == '{') depth++;
                else if (j[k] == '}') {
                    depth--;
                    if (depth == 0) {
                        k++;
                        break;
                    }
                }
            }
            std::string obj = j.substr(o, k - o);
            NodeSnapshot::PeerRow pr;
            pr.addr = jsonString(obj, "addr");
            pr.inbound = jsonBool(obj, "inbound");
            pr.ping = jsonDouble(obj, "pingtime");
            if (pr.ping <= 0) pr.ping = jsonDouble(obj, "minping");
            pr.version = jsonInt(obj, "version");
            pr.subver = jsonString(obj, "subver");
            pr.startingheight = jsonInt(obj, "startingheight");
            pr.synced_headers = jsonInt(obj, "synced_headers");
            pr.synced_blocks = jsonInt(obj, "synced_blocks");
            if (!pr.addr.empty()) {
                s.peers.push_back(pr);
                char line[256];
                std::snprintf(line, sizeof(line), "%s %s  ping=%.0fms  h=%d",
                              pr.inbound ? "in " : "out",
                              pr.addr.c_str(),
                              pr.ping * 1000.0,
                              pr.startingheight);
                s.peerLines.push_back(line);
            }
            i = k;
            if (s.peers.size() >= 64) break;
        }
        s.peerCount = (int)s.peers.size();
    }

    auto ibd = call("getibdinfo");
    if (ibd.ok && ibd.resultJson != "null" && ibd.resultJson.size() > 2) {
        s.hasIbdInfo = true;
        std::string tip = jsonString(ibd.resultJson, "tip_height");
        if (tip.empty()) tip = jsonString(ibd.resultJson, "blocks");
        std::string headers = jsonString(ibd.resultJson, "headers");
        std::string state = jsonString(ibd.resultJson, "status");
        if (state.empty()) state = jsonString(ibd.resultJson, "state");
        s.ibdSummary = "getibdinfo";
        if (!tip.empty()) s.ibdSummary += " tip=" + tip;
        if (!headers.empty()) s.ibdSummary += " headers=" + headers;
        if (!state.empty()) s.ibdSummary += " " + state;
        if (s.ibdSummary == "getibdinfo")
            s.ibdSummary = ibd.resultJson.substr(0, std::min<size_t>(160, ibd.resultJson.size()));

        s.assumeUtxoValidated = jsonBool(ibd.resultJson, "assumeutxo_validated");
        s.assumeUtxoFailed = jsonBool(ibd.resultJson, "assumeutxo_failed");
        s.assumeUtxoDualCollapsed = jsonBool(ibd.resultJson, "assumeutxo_dual_collapsed");
        s.assumeUtxoProgress = jsonDouble(ibd.resultJson, "assumeutxo_progress");
        s.snapshotChainstateActive = jsonBool(ibd.resultJson, "snapshot_active") ||
                                     jsonBool(ibd.resultJson, "assumeutxo_active") ||
                                     jsonBool(ibd.resultJson, "snapshot_chainstate_active");
    }

    auto mining = call("getmininginfo");
    if (mining.ok && mining.resultJson.size() > 2) {
        s.hasMining = true;
        s.miningBlocks = jsonInt(mining.resultJson, "blocks");
        s.difficulty = jsonDouble(mining.resultJson, "difficulty");
        s.networkHashPs = jsonDouble(mining.resultJson, "networkhashps");
        s.pooledTx = jsonInt(mining.resultJson, "pooledtx");
        s.miningChain = jsonString(mining.resultJson, "chain");
        s.miningErrors = jsonString(mining.resultJson, "errors");
    }

    return s;
}
