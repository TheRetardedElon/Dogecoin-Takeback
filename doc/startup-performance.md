# Startup & operations performance (Tier plan)

**Principle:** Make real work faster; splash/UI *inherits* shorter waits. Do not fake speed by skipping consensus, wallet integrity, or network rules.

Related: [client-startup-splash-breakdown.md](client-startup-splash-breakdown.md), [pro-gui-imgui.md](pro-gui-imgui.md).

## Tier 1 — done / in tree (safe, network-invisible)

| Change | Where | Effect |
|--------|--------|--------|
| DNS seed already on background thread | `CConnman::Start` → `ThreadDNSAddressSeed` | Does not block `Done loading` |
| No second `getaddrinfo` on seed *name* | `net.cpp` — dummy RFC5737 source | Faster/safer dnsseed thread |
| Defer `-externalip` hostname DNS | `init.cpp` background thread | AppInit not stuck on resolver |
| Defer `Discover()` (Windows hostname DNS) | `init.cpp` | Splash not blocked on LAN discovery |
| SoftSet `-dbcache=1024` on Prefer Fast Sync | `intro.cpp` (if unset) | Faster IBD/UTXO flushes post Fast Sync |
| Fast Sync automation | prior work | Quiet net, mid-IBD gate, no AbortNode on body-less tip |
| ImGui Path A splash | `pro-gui` | Waits on RPC only; AppInit stays in `dogecoind` |

## Tier 2 — next (still consensus-clean)

- Profile cold start: block index open vs `CVerifyDB` vs wallet rescan  
- Optional parallel verify (same checks)  
- Full JSON-RPC client in pro-gui (balance, peers, Fast Sync status)

## Tier 3 — structural (flag + migration)

- `CDBWrapper` talks to a byte `CDbBackend` (`src/dbengine.*`). **Default for a new/empty dir is still LevelDB.**  
- If `-dbengine` is **unset**, an existing `ENGINE` stamp is honored — a swapped MDBX datadir opens without a flag.  
- `-dbengine=mdbx` on an empty dir (or wipe). Existing LevelDB folders are refused.  
- `-migratedb=mdbx` copies chainstate + block index as raw bytes to sibling `*_mdbx` folders.  
- `-migratedb=mdbx -swapdb` then renames the live folders aside and continues startup on MDBX.  
- `getdbengine` / `getblockchaininfo.dbengine` report what is on disk.  
- No in-place convert of a live LevelDB folder. Network-invisible. Consensus untouched.

## What still dominates splash time (Qt full client)

1. Load block index + open chainstate LevelDB  
2. Wallet verify/load/rescan (when needed)  
3. Optional verify depth (`-checkblocks`)  
4. *Not* DNS seeds (already async) once Tier 1 discover/externalip fixes land  

## Measure

```text
# After a run, inspect debug.log timings:
#  "Loaded N addresses from peers.dat  Xms"
#  " block index            Nms"
#  "DNS seed … in Xms"
```

Never optimize without a before/after on a real datadir.
