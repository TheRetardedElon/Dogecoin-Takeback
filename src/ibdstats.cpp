// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ibdstats.h"

#include "util.h"
#include "utiltime.h"

#include <univalue.h>

namespace IBDStats {

namespace {

std::atomic<uint64_t> g_blocks_requested{0};
std::atomic<uint64_t> g_blocks_received{0};
std::atomic<uint64_t> g_blocks_connected{0};
std::atomic<uint64_t> g_stall_starts{0};
std::atomic<uint64_t> g_stall_disconnects{0};
std::atomic<uint64_t> g_download_timeouts{0};
std::atomic<uint64_t> g_headers_timeouts{0};
std::atomic<uint64_t> g_ibd_rescue_fetches{0};
std::atomic<uint64_t> g_flushes{0};
std::atomic<uint64_t> g_flush_total_us{0};
std::atomic<uint64_t> g_flush_max_us{0};
std::atomic<uint64_t> g_flush_last_cache_bytes{0};
std::atomic<uint64_t> g_connect_total_us{0};
std::atomic<uint64_t> g_connect_max_us{0};
std::atomic<int> g_last_tip_height{0};
std::atomic<int64_t> g_last_tip_time_us{0};
// C++11: value-initialize atomics (default ctor leaves them indeterminate).
std::atomic<uint64_t> g_flush_by_reason[FLUSH_REASON_COUNT] = {};

std::atomic<int64_t> g_last_progress_log_us{0};

void UpdateMax(std::atomic<uint64_t>& max_atom, uint64_t value)
{
    uint64_t prev = max_atom.load(std::memory_order_relaxed);
    while (value > prev && !max_atom.compare_exchange_weak(prev, value, std::memory_order_relaxed)) {
        // retry
    }
}

} // anon namespace

void NoteBlockRequested()
{
    g_blocks_requested.fetch_add(1, std::memory_order_relaxed);
}

void NoteBlockReceived()
{
    g_blocks_received.fetch_add(1, std::memory_order_relaxed);
}

void NoteBlockConnected(int64_t connect_us, int height)
{
    g_blocks_connected.fetch_add(1, std::memory_order_relaxed);
    if (connect_us > 0) {
        g_connect_total_us.fetch_add(static_cast<uint64_t>(connect_us), std::memory_order_relaxed);
        UpdateMax(g_connect_max_us, static_cast<uint64_t>(connect_us));
    }
    NoteTipAdvanced(height, GetTimeMicros());
}

void NoteStallStarted(int node_id)
{
    g_stall_starts.fetch_add(1, std::memory_order_relaxed);
    LogPrint("ibd", "ibd: stall started peer=%d (total_starts=%llu)\n",
             node_id,
             (unsigned long long)g_stall_starts.load(std::memory_order_relaxed));
}

void NoteStallDisconnect(int node_id)
{
    g_stall_disconnects.fetch_add(1, std::memory_order_relaxed);
    LogPrintf("ibd: stall disconnect peer=%d (total_stall_disconnects=%llu)\n",
              node_id,
              (unsigned long long)g_stall_disconnects.load(std::memory_order_relaxed));
}

void NoteDownloadTimeout(int node_id)
{
    g_download_timeouts.fetch_add(1, std::memory_order_relaxed);
    LogPrintf("ibd: download timeout peer=%d (total_dl_timeouts=%llu)\n",
              node_id,
              (unsigned long long)g_download_timeouts.load(std::memory_order_relaxed));
}

void NoteHeadersTimeout(int node_id)
{
    g_headers_timeouts.fetch_add(1, std::memory_order_relaxed);
    LogPrintf("ibd: headers timeout peer=%d (total_hdr_timeouts=%llu)\n",
              node_id,
              (unsigned long long)g_headers_timeouts.load(std::memory_order_relaxed));
}

void NoteIbdRescueFetch(int node_id)
{
    g_ibd_rescue_fetches.fetch_add(1, std::memory_order_relaxed);
    LogPrint("ibd", "ibd: rescue fetch enabled for non-preferred peer=%d\n", node_id);
}

void NoteFlush(int64_t duration_us, size_t cache_bytes, FlushReason reason)
{
    g_flushes.fetch_add(1, std::memory_order_relaxed);
    if (duration_us > 0) {
        g_flush_total_us.fetch_add(static_cast<uint64_t>(duration_us), std::memory_order_relaxed);
        UpdateMax(g_flush_max_us, static_cast<uint64_t>(duration_us));
    }
    g_flush_last_cache_bytes.store(static_cast<uint64_t>(cache_bytes), std::memory_order_relaxed);
    if (reason >= 0 && reason < FLUSH_REASON_COUNT) {
        g_flush_by_reason[reason].fetch_add(1, std::memory_order_relaxed);
    }
    LogPrint("ibd", "ibd: flush reason=%s duration_ms=%.2f cache_mb=%.2f total_flushes=%llu\n",
             FlushReasonName(reason).c_str(),
             duration_us / 1000.0,
             cache_bytes / (1024.0 * 1024.0),
             (unsigned long long)g_flushes.load(std::memory_order_relaxed));
}

void NoteTipAdvanced(int height, int64_t now_us)
{
    g_last_tip_height.store(height, std::memory_order_relaxed);
    g_last_tip_time_us.store(now_us, std::memory_order_relaxed);
}

Snapshot GetSnapshot()
{
    Snapshot s;
    s.blocks_requested = g_blocks_requested.load(std::memory_order_relaxed);
    s.blocks_received = g_blocks_received.load(std::memory_order_relaxed);
    s.blocks_connected = g_blocks_connected.load(std::memory_order_relaxed);
    s.stall_starts = g_stall_starts.load(std::memory_order_relaxed);
    s.stall_disconnects = g_stall_disconnects.load(std::memory_order_relaxed);
    s.download_timeouts = g_download_timeouts.load(std::memory_order_relaxed);
    s.headers_timeouts = g_headers_timeouts.load(std::memory_order_relaxed);
    s.ibd_rescue_fetches = g_ibd_rescue_fetches.load(std::memory_order_relaxed);
    s.flushes = g_flushes.load(std::memory_order_relaxed);
    s.flush_total_us = g_flush_total_us.load(std::memory_order_relaxed);
    s.flush_max_us = g_flush_max_us.load(std::memory_order_relaxed);
    s.flush_last_cache_bytes = g_flush_last_cache_bytes.load(std::memory_order_relaxed);
    s.connect_total_us = g_connect_total_us.load(std::memory_order_relaxed);
    s.connect_max_us = g_connect_max_us.load(std::memory_order_relaxed);
    s.last_tip_height = g_last_tip_height.load(std::memory_order_relaxed);
    s.last_tip_time_us = g_last_tip_time_us.load(std::memory_order_relaxed);
    for (int i = 0; i < FLUSH_REASON_COUNT; ++i) {
        s.flush_by_reason[i] = g_flush_by_reason[i].load(std::memory_order_relaxed);
    }
    return s;
}

std::string FlushReasonName(FlushReason r)
{
    switch (r) {
    case FLUSH_ALWAYS: return "always";
    case FLUSH_CACHE_LARGE: return "cache_large";
    case FLUSH_CACHE_CRITICAL: return "cache_critical";
    case FLUSH_PERIODIC: return "periodic";
    case FLUSH_PRUNE: return "prune";
    default: return "other";
    }
}

UniValue ToUniValue(const Snapshot& s)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("blocks_requested", s.blocks_requested);
    obj.pushKV("blocks_received", s.blocks_received);
    obj.pushKV("blocks_connected", s.blocks_connected);
    obj.pushKV("stall_starts", s.stall_starts);
    obj.pushKV("stall_disconnects", s.stall_disconnects);
    obj.pushKV("download_timeouts", s.download_timeouts);
    obj.pushKV("headers_timeouts", s.headers_timeouts);
    obj.pushKV("ibd_rescue_fetches", s.ibd_rescue_fetches);
    obj.pushKV("flushes", s.flushes);
    obj.pushKV("flush_total_ms", s.flush_total_us / 1000.0);
    obj.pushKV("flush_max_ms", s.flush_max_us / 1000.0);
    obj.pushKV("flush_last_cache_bytes", s.flush_last_cache_bytes);
    obj.pushKV("connect_total_ms", s.connect_total_us / 1000.0);
    obj.pushKV("connect_max_ms", s.connect_max_us / 1000.0);
    if (s.blocks_connected > 0) {
        obj.pushKV("connect_avg_ms", (s.connect_total_us / 1000.0) / s.blocks_connected);
    }
    obj.pushKV("last_tip_height", s.last_tip_height);
    if (s.last_tip_time_us > 0) {
        obj.pushKV("last_tip_time", s.last_tip_time_us / 1000000);
    }

    UniValue reasons(UniValue::VOBJ);
    reasons.pushKV("always", s.flush_by_reason[FLUSH_ALWAYS]);
    reasons.pushKV("cache_large", s.flush_by_reason[FLUSH_CACHE_LARGE]);
    reasons.pushKV("cache_critical", s.flush_by_reason[FLUSH_CACHE_CRITICAL]);
    reasons.pushKV("periodic", s.flush_by_reason[FLUSH_PERIODIC]);
    reasons.pushKV("prune", s.flush_by_reason[FLUSH_PRUNE]);
    reasons.pushKV("other", s.flush_by_reason[FLUSH_OTHER]);
    obj.pushKV("flush_by_reason", reasons);
    return obj;
}

void MaybeLogProgress(int tip_height, int header_height, size_t blocks_in_flight,
                      int preferred_download, int peers_with_downloads)
{
    if (!LogAcceptCategory("ibd"))
        return;
    const int64_t now = GetTimeMicros();
    int64_t prev = g_last_progress_log_us.load(std::memory_order_relaxed);
    // At most once per 30s
    if (prev != 0 && now - prev < 30 * 1000000)
        return;
    if (!g_last_progress_log_us.compare_exchange_strong(prev, now, std::memory_order_relaxed))
        return;

    Snapshot s = GetSnapshot();
    LogPrint("ibd",
             "ibd: progress tip=%d headers=%d inflight=%u preferred=%d dl_peers=%d "
             "recv=%llu conn=%llu stall_disc=%llu dl_to=%llu flushes=%llu rescue=%llu\n",
             tip_height, header_height, (unsigned)blocks_in_flight, preferred_download,
             peers_with_downloads,
             (unsigned long long)s.blocks_received,
             (unsigned long long)s.blocks_connected,
             (unsigned long long)s.stall_disconnects,
             (unsigned long long)s.download_timeouts,
             (unsigned long long)s.flushes,
             (unsigned long long)s.ibd_rescue_fetches);
}

} // namespace IBDStats
