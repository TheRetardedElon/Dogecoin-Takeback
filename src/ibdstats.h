// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_IBDSTATS_H
#define DOGECOIN_IBDSTATS_H

#include <atomic>
#include <cstdint>
#include <string>

class UniValue;

/**
 * Lightweight Initial Block Download / peer-delivery telemetry.
 *
 * Design goals for the 1.14-based tree:
 *  - Zero consensus impact
 *  - No extra locks on the hot path (atomics only)
 *  - Cheap enough to leave enabled always; detailed logs gated by -debug=ibd
 *
 * Does NOT attempt true LevelDB write-amplification (requires engine internals).
 * Instead we record flush duration, cache size at flush, and flush reasons —
 * which is what operators need to tune -dbcache / pruning.
 */
namespace IBDStats {

enum FlushReason {
    FLUSH_ALWAYS = 0,
    FLUSH_CACHE_LARGE,
    FLUSH_CACHE_CRITICAL,
    FLUSH_PERIODIC,
    FLUSH_PRUNE,
    FLUSH_OTHER,
    FLUSH_REASON_COUNT
};

void NoteBlockRequested();
void NoteBlockReceived();
void NoteBlockConnected(int64_t connect_us, int height);
void NoteStallStarted(int node_id);
void NoteStallDisconnect(int node_id);
void NoteDownloadTimeout(int node_id);
void NoteHeadersTimeout(int node_id);
void NoteIbdRescueFetch(int node_id);
void NoteFlush(int64_t duration_us, size_t cache_bytes, FlushReason reason);
void NoteTipAdvanced(int height, int64_t now_us);

/** Snapshot for RPC / logging. */
struct Snapshot {
    uint64_t blocks_requested;
    uint64_t blocks_received;
    uint64_t blocks_connected;
    uint64_t stall_starts;
    uint64_t stall_disconnects;
    uint64_t download_timeouts;
    uint64_t headers_timeouts;
    uint64_t ibd_rescue_fetches;
    uint64_t flushes;
    uint64_t flush_total_us;
    uint64_t flush_max_us;
    uint64_t flush_last_cache_bytes;
    uint64_t connect_total_us;
    uint64_t connect_max_us;
    int last_tip_height;
    int64_t last_tip_time_us;
    uint64_t flush_by_reason[FLUSH_REASON_COUNT];
};

Snapshot GetSnapshot();
UniValue ToUniValue(const Snapshot& s);

/** Periodic progress line when -debug=ibd (rate-limited). */
void MaybeLogProgress(int tip_height, int header_height, size_t blocks_in_flight,
                      int preferred_download, int peers_with_downloads);

std::string FlushReasonName(FlushReason r);

} // namespace IBDStats

#endif // DOGECOIN_IBDSTATS_H
