# P0: IBD Telemetry & Peer Stall Hardening

**Status:** implemented (first cut)  
**Scope:** non-consensus — policy, logging, RPC, download peer selection  
**Tree:** Dogecoin Core Pro / Takeback (1.14.x DNA)

## Why this first

Modern features (AssumeUTXO, RocksDB, BIP 324) need a modular, measurable node.
On 1.14 DNA we still have a single chainstate under `cs_main`. Jumping to dual
chainstate without fixing “stuck at 87%” and without measurements is how you
ship silent corruption and un-debuggable regressions.

P0 goals:

1. **See** where IBD spends time (connect vs flush vs peer stall).
2. **Unstick** IBD when preferred download peers stall.
3. **Never** change block acceptance, AuxPoW, subsidy, or script rules.

## Architecture reality (1.14)

| Modern Bitcoin | This tree |
|----------------|-----------|
| `ChainstateManager` dual chainstates | Single global `chainActive` / `pcoinsTip` |
| `node/blockstorage` | Logic in `validation.cpp` |
| AssumeUTXO | Not present |
| Separate net processing package | `net_processing.cpp` + `net.cpp` |

Key existing pieces we **build on** (not reinvent):

- `CNodeState` in `net_processing.cpp` (anon namespace, `cs_main`)
- `mapBlocksInFlight`, `nStallingSince`, `nDownloadingSince`
- `FindNextBlocksToDownload` + `BLOCK_DOWNLOAD_WINDOW`
- Stall disconnect: `BLOCK_STALLING_TIMEOUT` (2s window stall)
- Per-block timeout: `BLOCK_DOWNLOAD_TIMEOUT_*` × `nPowTargetSpacing` (60s on mainnet → ~5 min base)

## File surface

| File | Role |
|------|------|
| `src/ibdstats.h` / `src/ibdstats.cpp` | Atomic counters; RPC snapshot; `-debug=ibd` logs |
| `src/net_processing.cpp` | Stall / timeout counters; IBD rescue fetch; progress log |
| `src/net_processing.h` | Extra `CNodeStateStats` fields for `getpeerinfo` |
| `src/validation.cpp` | Flush timing + connect timing into IBDStats |
| `src/rpc/blockchain.cpp` | `getibdinfo` RPC |
| `src/Makefile.am` | Build wiring |

Not touched: `auxpow.*`, `pow.*`, `dogecoin.cpp` consensus checks, chainparams consensus params.

## 1. Telemetry design

### Hot path rules

- Counters are `std::atomic` with `memory_order_relaxed`.
- No new mutexes in `ConnectTip` / `SendMessages` / `FlushStateToDisk`.
- Expensive formatted logs only under `-debug=ibd` (or `LogPrintf` on rare disconnect events).

### What we measure

| Metric | Source | Why |
|--------|--------|-----|
| `blocks_requested` / `blocks_received` | net_processing | Peer delivery vs request rate |
| `blocks_connected` + connect µs | ConnectTip | Validation CPU bound? |
| `stall_starts` / `stall_disconnects` | SendMessages | “Stuck window” peers |
| `download_timeouts` | front-of-queue timeout | Slow peer holding a height |
| `headers_timeouts` | headers sync path | Header stall |
| `ibd_rescue_fetches` | rescue path | How often preferred peers failed us |
| `flushes` + duration + cache size + reason | FlushStateToDisk | dbcache / I/O spikes |

### What we do **not** measure yet

- True LevelDB write amplification (needs LevelDB stats hooks / RocksDB era).
- Per-script-check queue latency (can be P0.1 if connect is hot).

### Operator usage

```bash
# Detailed IBD log lines
dogecoind -debug=ibd

# Snapshot counters (safe anytime)
dogecoin-cli getibdinfo

# Per-peer flight / stall hints
dogecoin-cli getpeerinfo
```

## 2. Peer stall & rotation logic

### Already in tree (Bitcoin 0.14 inheritance)

1. **Window stall** (`nStallingSince`): when we cannot advance the download
   window because a peer holds the next block, start a timer; after
   `BLOCK_STALLING_TIMEOUT` (2s) disconnect that peer.
2. **Front-of-queue timeout**: if the oldest in-flight block exceeds
   `nPowTargetSpacing * (BASE + PER_PEER * others)`, disconnect.
3. **On disconnect** (`FinalizeNode`): erase that peer’s entries from
   `mapBlocksInFlight` so other peers may request those hashes.
4. **Preferred download**: during IBD, only `fPreferredDownload` peers
   (outbound / whitelist) fetch blocks — unless there are zero preferred peers.

### P0 change: IBD rescue fetch

Problem: if preferred peers are **stalling or fully saturated**, IBD waits on
them even when good inbound (or non-preferred) peers could serve blocks.

When `IsInitialBlockDownload()` and this peer is not preferred:

- If any preferred peer has `nStallingSince != 0`, **or**
- All preferred peers are at `MAX_BLOCKS_IN_TRANSIT_PER_PEER` while work remains,

then allow this non-client, non-one-shot peer to participate in
`FindNextBlocksToDownload` for this `SendMessages` tick.

Still **one in-flight owner per block hash** (no double-download waste).
Still disconnects stallers so their maps clear.

This is policy-only and matches “drop-and-rotate” without a ChainstateManager.

### Drop-and-rotate sequence (runtime)

```
preferred peer advertises headers
  → we MarkBlockAsInFlight(preferred)
  → peer stalls (no BLOCK)
  → nStallingSince set when window cannot move
  → after 2s: fDisconnect = true, IBDStats::NoteStallDisconnect
  → FinalizeNode clears mapBlocksInFlight for that peer
  → next SendMessages: other peers (incl. rescue) request same heights
```

## 3. Structs / state touched

### `CNodeState` (net_processing.cpp, already exists)

Relevant fields (unchanged layout; we only read them more carefully):

- `nStallingSince`, `nDownloadingSince`
- `vBlocksInFlight`, `nBlocksInFlight`, `nBlocksInFlightValidHeaders`
- `fPreferredDownload`, `pindexLastCommonBlock`, `pindexBestKnownBlock`

### `CNodeStateStats` (exported)

Extended for RPC:

- `nBlocksInFlight`
- `fPreferredDownload`
- `fStalling` / `nStallingSeconds` (derived from `nStallingSince`)

### New: `IBDStats::*` atomics

Process-global, not per-peer (keeps `CNode` / wire protocol clean).

## 4. Explicit non-goals (this PR / P0)

- AssumeUTXO / dual chainstate  
- RocksDB / zstd  
- BIP 324 / ASMAP / Erlay / BIP 157  
- Changing `BLOCK_STALLING_TIMEOUT` defaults without field data from `getibdinfo`  
- Consensus or AuxPoW changes  

## 5. Follow-ups

### P0.1 (implemented)

1. ~~GUI: stall/rescue on Network page~~  
2. ~~`-ibdrescue` kill-switch~~  
3. **IBD-aware chainstate flush policy** — soft flush only near 95% cache full during IBD; 2× periodic flush interval; still critical/prune/always.  
4. **Prune UX** — clearer help, reindex warnings, InitWarning for tight automatic targets (&lt; 5500 MiB).  
5. **Startup tips** — log `-dbcache` guidance when unset; prune mode note.  
6. **Outbound diversity (ASMAP-lite)** — soft avoid stacking &gt;2 outbounds in same IPv4 /8; try up to 200 addrman candidates.  
7. **`getibdinfo`** — prune / dbcache budget / size_on_disk / ibd_rescue fields.  
8. **`-debug=ibd`** listed in help categories.

### Later

1. Adaptive `MAX_BLOCKS_IN_TRANSIT_PER_PEER` under high RTT (data-driven).  
2. Full **ASMAP** ASN maps (replaces /8 soft preference).  
3. Design doc for AssumeUTXO **after** multi-node IBD telemetry samples.

## 6. Test plan

- Regtest: sync with one slow peer (traffic control / artificial delay) →
  stall disconnect increments; tip still advances via second peer.  
- Mainnet IBD sample: `getibdinfo` every 10 minutes; confirm flushes and
  connect_avg_ms are plausible; rescue count non-zero only when preferred peers
  misbehave.  
- `-reindex` / prune mode: no new crashes; flush_by_reason.prune increments.

## 7. Success criteria

- Operators can answer “why am I stuck?” with `getibdinfo` + `getpeerinfo`.  
- Preferred-peer stall no longer permanently pins IBD when alternatives exist.  
- Zero consensus rule changes; AuxPoW path untouched.
