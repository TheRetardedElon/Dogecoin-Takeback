#pragma once

#include "rpc_client.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

/** Background RPC snapshot so the ImGui thread never blocks on connect/recv. */
class RpcProbeWorker {
public:
    RpcProbeWorker() = default;
    ~RpcProbeWorker() { Stop(); }

    RpcProbeWorker(const RpcProbeWorker&) = delete;
    RpcProbeWorker& operator=(const RpcProbeWorker&) = delete;

    void Start();
    void Stop();
    void SetConfig(const RpcConfig& cfg);
    void SetWake(void (*fn)());
    void Kick();
    bool Consume(NodeSnapshot& out);
    bool Busy() const { return busy.load(); }

private:
    void Loop();

    RpcConfig cfg;
    NodeSnapshot latest;
    unsigned gen = 0;
    unsigned seen = 0;
    void (*wake)() = nullptr;
    std::mutex mu;
    std::condition_variable cv;
    std::thread th;
    std::atomic<bool> running{false};
    std::atomic<bool> busy{false};
    std::atomic<bool> kick{false};
};
