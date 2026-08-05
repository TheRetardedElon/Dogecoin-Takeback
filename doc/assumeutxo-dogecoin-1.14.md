# AssumeUTXO for Dogecoin Core (1.14 DNA) — Design

**Status:** design only (not implemented)  
**Depends on:** P0/P0.1 IBD telemetry, ASMAP, healthy single-chainstate  
**Consensus impact:** none if done like Bitcoin (background full validation to the assume height)

## 1. Goal

Make a new full node **usable in minutes** (wallet, tip, mempool) while still **fully validating** history in the background — without changing AuxPoW, subsidy, or script rules.

Today (1.14): sequential IBD from genesis (or pruned tip) → long wait before the wallet is trustworthy.

AssumeUTXO (Bitcoin Core model):

1. Load a **serialized UTXO snapshot** for a hardcoded, widely attested height `H`.
2. Instantly activate a **snapshot chainstate** at `H` and sync headers/blocks **above** `H`.
3. In the background, validate blocks **from genesis → H** into a second chainstate and **prove** it matches the snapshot hash.
4. When background validation completes, drop the temporary dual state and continue as a normal node.

## 2. Why this is hard on 1.14 Dogecoin DNA

| Modern Bitcoin | This tree |
|----------------|-----------|
| `ChainstateManager` + dual `CChainState` | Single global `chainActive`, `pcoinsTip`, `mapBlockIndex` under `cs_main` |
| Snapshot load + background sync modules | No `node/utxo_snapshot`, no dual tip |
| Clear separation of “active tip” vs “validation tip” | `ActivateBestChain` assumes one tip |

**You cannot bolt AssumeUTXO onto `main.cpp`-era globals without a multi-PR refactor.** Gemini/Grok agreement: dual chainstate first.

## 3. Trust model (must document for users)

- Snapshot is **not** a soft fork. It is a **bootstrap optimization**.
- Trust is temporary: node only “assumes” until background validation finishes.
- Hardcoded: `assumeutxo` hash + height in `chainparams` (mainnet/testnet/regtest separately).
- Operators can still full-sync with `-assumeutxo=0` / no snapshot (name TBD).
- Dogecoin-specific: snapshot height should be **post-AuxPoW** (after 371337) so historical AuxPoW edge cases are in the background path, not the “instant tip” path — or include AuxPoW era with full validation rules (preferred: any height with correct `CheckAuxPowProofOfWork`).

## 4. Phased PR plan

### Phase A — Dual chainstate foundation (largest)

**Objective:** ability to hold two coins views / tips without corrupting reorgs.

Suggested structure (names illustrative):

| New / changed | Role |
|---------------|------|
| `src/node/chainstate.h/.cpp` | Own `CChain`, coins tip, optional block index views |
| `src/validation.cpp` | Move tip activation behind “active chainstate” pointer |
| `src/init.cpp` | Construct manager; shutdown flush both |
| Call sites | RPCs, wallet, net_processing use **active** chainstate |

**Exit criteria:** single-chainstate behavior bit-identical; regtest + mainnet IBD green; no AssumeUTXO yet.

### Phase B — Snapshot file format + load

**Format (align with Bitcoin where practical):**

- Magic + version  
- Network magic / chain  
- Base block hash + height  
- Coins count  
- LevelDB-style or custom coin serialization stream  
- Trailing hash of contents (must match hardcoded assume hash)

**Code:**

| Piece | Role |
|-------|------|
| `src/node/utxo_snapshot.h/.cpp` | Serialize/deserialize |
| RPC `loadtxoutset` (or dogecoin name) | Operator loads snapshot file |
| `chainparams` | `AssumeutxoData { height, hash_serialized }` |

**Exit criteria:** load snapshot on regtest; tip at H; wallet sees UTXOs; **without** background validation yet (dev-only flag).

### Phase C — Background validation

| Piece | Role |
|-------|------|
| Second chainstate from genesis | Download/validate blocks 0…H |
| Progress RPC / `getibdinfo` | `snapshot_height`, `background_synced`, `assumeutxo_validated` |
| Completion | Compare coins hash to assume hash; on match, promote; on mismatch, abort/require reindex |

**Exit criteria:** intentional bad snapshot fails closed; good snapshot validates and converges.

### Phase D — Product polish

- GUI modal: “Using snapshot; historical validation N%”  
- Prune interaction rules (snapshot + prune is subtle — design carefully)  
- Release tooling: produce signed snapshot artifacts for height H  
- Docs + security FAQ  

## 5. Dogecoin-specific checklist

- [ ] AuxPoW blocks validate in background path via existing `CheckAuxPowProofOfWork`  
- [ ] Digishield / subsidy epochs correct across H  
- [ ] Pruned nodes: snapshot must not require missing block files for tip operation  
- [ ] 1-minute block spacing → choose H with stable community attestation  
- [ ] Snapshot size estimate & hosting (CDN / torrent / GitHub release)  
- [ ] No change to AuxPoW chain ID, merge-mining magic, or consensus constants  

## 6. File surface estimate (1.14)

**High churn:**  
`validation.cpp/h`, `init.cpp`, `txdb.cpp/h`, `coins.cpp/h`, `net_processing.cpp`, `rpc/blockchain.cpp`, `chainparams.cpp`, `qt/clientmodel` / modal overlay  

**New:**  
`node/chainstate.*`, `node/utxo_snapshot.*`, tests under `src/test/`  

**Untouched (goal):**  
`auxpow.*`, `pow.*` subsidy, script interpreter consensus  

## 7. What we will **not** do in Phase A–C

- RocksDB migration  
- Utreexo / ZK proofs  
- Soft-forking a consensus-committed UTXO root (different project)  

## 8. Prerequisites already done in this repo

| Prerequisite | Status |
|--------------|--------|
| IBD telemetry (`getibdinfo`) | Done |
| Peer stall + rescue | Done |
| IBD flush policy | Done |
| ASMAP peer diversity | Done |
| IBD parallel download (P0.3) | In progress alongside this doc |

## 9. Suggested next engineering after this doc is accepted

1. Land P0.3 download parallelism  
2. Spike branch: extract minimal “active chainstate” pointer (Phase A slice 1)  
3. Regtest dual tip smoke test  
4. Only then snapshot load (Phase B)  

## 10. Success metric

A laptop with a published mainnet snapshot reaches **wallet-usable tip in &lt; 30 minutes** on a typical home connection, with background validation completing later without user action, and a wrong snapshot **never** producing a silent wrong chain.
