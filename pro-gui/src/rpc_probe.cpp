#include "rpc_probe.h"

#include <chrono>

void RpcProbeWorker::Start()
{
    if (running.exchange(true))
        return;
    th = std::thread([this] { Loop(); });
}

void RpcProbeWorker::Stop()
{
    if (!running.exchange(false))
        return;
    cv.notify_all();
    if (th.joinable())
        th.join();
}

void RpcProbeWorker::SetConfig(const RpcConfig& c)
{
    std::lock_guard<std::mutex> lock(mu);
    cfg = c;
}

void RpcProbeWorker::SetWake(void (*fn)())
{
    wake = fn;
}

void RpcProbeWorker::Kick()
{
    kick.store(true);
    cv.notify_all();
}

bool RpcProbeWorker::Consume(NodeSnapshot& out)
{
    std::lock_guard<std::mutex> lock(mu);
    if (gen == seen)
        return false;
    seen = gen;
    out = latest;
    return true;
}

void RpcProbeWorker::Loop()
{
    unsigned idleFails = 0;
    while (running.load()) {
        RpcConfig local;
        {
            std::lock_guard<std::mutex> lock(mu);
            local = cfg;
        }
        busy.store(true);
        RpcClient client(local);
        NodeSnapshot snap = client.refreshSnapshot();
        busy.store(false);
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(mu);
            latest = std::move(snap);
            ++gen;
            ok = latest.connected;
        }
        if (wake)
            wake();
        idleFails = ok ? 0 : idleFails + 1;
        int waitMs = ok ? 600 : (idleFails < 4 ? 700 : 1200);
        if (kick.exchange(false))
            waitMs = 50;

        std::unique_lock<std::mutex> lock(mu);
        cv.wait_for(lock, std::chrono::milliseconds(waitMs),
                    [this] { return !running.load() || kick.load(); });
    }
}
