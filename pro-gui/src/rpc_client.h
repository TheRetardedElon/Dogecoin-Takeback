#pragma once

#include <string>
#include <vector>

/** Minimal Dogecoin JSON-RPC over HTTP (cookie or user:pass). No Qt. */
struct RpcConfig {
    std::string host = "127.0.0.1";
    int port = 22555;
    std::string user;
    std::string password;
    std::string cookiePath; // empty = try default datadir cookies
};

struct RpcResult {
    bool ok = false;
    int httpCode = 0;
    std::string body;     // full HTTP body (headers stripped when possible)
    std::string error;    // transport or RPC error message
    std::string resultJson; // value of "result" if present
};

/** Live snapshot for ImGui panels (Path A control plane). */
struct NodeSnapshot {
    bool connected = false;
    bool rpcWarmup = false;
    std::string status;

    // getblockchaininfo
    std::string chain;
    int blocks = -1;
    int headers = -1;
    double verificationProgress = 0.0;
    bool initialBlockDownload = false;
    std::string bestBlockHash;
    std::string dbEngine; // leveldb | mdbx | none

    // getnetworkinfo
    int connections = -1;
    int version = 0;
    std::string subversion;
    std::string networkActive; // "true"/"false" string for display
    std::string p2pProxy;      // from getnetworkinfo networks[].proxy
    std::string onionAddress;  // local .onion if any

    // getwalletinfo (optional — node may be -disablewallet)
    bool hasWallet = false;
    double balance = 0.0;
    double unconfirmed = 0.0;
    int walletVersion = 0;
    std::string walletName;
    bool walletEncrypted = false;
    bool walletLocked = false;

    // getpeerinfo
    struct PeerRow {
        std::string addr;
        bool inbound = false;
        double ping = 0.0;
        int version = 0;
        std::string subver;
        int startingheight = -1;
        int synced_headers = -1;
        int synced_blocks = -1;
    };
    int peerCount = 0;
    std::vector<std::string> peerLines; // short rows for table
    std::vector<PeerRow> peers;

    // getibdinfo if available (Core Pro)
    bool hasIbdInfo = false;
    std::string ibdSummary;
    bool assumeUtxoValidated = false;
    bool assumeUtxoFailed = false;
    bool assumeUtxoDualCollapsed = false;
    double assumeUtxoProgress = 0.0;
    bool snapshotChainstateActive = false;

    // getmininginfo
    bool hasMining = false;
    int miningBlocks = -1;
    double difficulty = 0.0;
    double networkHashPs = 0.0;
    int pooledTx = -1;
    std::string miningChain;
    std::string miningErrors;
};

class RpcClient {
public:
    explicit RpcClient(RpcConfig cfg = {});

    void setConfig(const RpcConfig& cfg) { m_cfg = cfg; }
    const RpcConfig& config() const { return m_cfg; }

    bool portOpen(int timeoutMs = 400) const;

    /** JSON-RPC call. paramsJson is raw JSON array, e.g. "[]" or "[\"addr\"]". */
    RpcResult call(const std::string& method, const std::string& paramsJson = "[]") const;

    /** Refresh multi-call snapshot for the shell. */
    NodeSnapshot refreshSnapshot() const;

    static std::string jsonString(const std::string& json, const char* key);
    static int jsonInt(const std::string& json, const char* key, int def = -1);
    static double jsonDouble(const std::string& json, const char* key, double def = 0.0);
    static bool jsonBool(const std::string& json, const char* key, bool def = false);

private:
    RpcConfig m_cfg;
    std::string resolveAuthHeader() const;
};
