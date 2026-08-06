# AssumeUTXO for Dogecoin Core (1.14 DNA) — Design

**Status:** design + **Phase D3 prune guard + regtest smoke**  
**Depends on:** P0/P0.1 IBD telemetry, ASMAP, healthy single-chainstate  
**Consensus impact:** none if done like Bitcoin (background full validation to the assume height)

### Implementation progress

| Slice | Status | Notes |
|-------|--------|--------|
| **A1** Active chainstate wrapper | **Done** | `src/node/chainstate.*`, `InitializeActiveChainstate()`, `getchainstates`, `getibdinfo` fields |
| **A2** Migrate hot paths to ActiveChain*() | **Done** | blockchain + mining RPCs, clientmodel; lazy init; TestingSetup hook |
| **A3** Dual CChainState storage (no snapshot yet) | **Done** | idle `background` owns empty `CChain`; no coins DB; `getchainstates` lists both |
| **B1** Snapshot file format + dump/load | **Done** | `node/utxo_snapshot.*`, `dumptxoutset` / `loadtxoutset`; loads into `chainstate_snapshot/` on background |
| **B2** Activate snapshot as tip (dev) | **Done** | `activatesnapshot` / `loadtxoutset … true`; `-assumeutxodev`; mirrors `chainActive`/`pcoinsTip`; parks IBD on background |
| **C1** Background validation | **Done** | `StepBackgroundValidation` ConnectBlock loop; `hash_serialized` match; fail-closed shutdown; auto-step after ActivateBestChain; `stepbackgroundvalidation` RPC |
| **C2** Persist / restore / fetch / collapse | **Done** | `assumeutxo.dat`; `MaybeRestoreAssumeUtxo` on startup; historical getdata for missing blocks; dual collapse after validated |
| **D1** Attestation gate + GUI progress | **Done** | `mapAssumeutxo` / `AssumeutxoData` in chainparams; activate if attested **or** `-assumeutxodev`; status-bar historical % |
| **D2** Attestation workflow + hard collapse | **Done** | `dumptxoutset` → `hash_serialized` + snippet; `listassumeutxo`; hard-collapse background after prove |
| **D3** Prune guard + regtest smoke | **Done** | refuse `pruneblockchain` during bg proof; `qa/rpc-tests/assumeutxo.py` |

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
| `src/node/chainstate.h/.cpp` | **A1 done:** wraps `chainActive` / `pcoinsTip` as `CChainState` named `"ibd"` |
| `src/validation.cpp` | Move tip activation behind “active chainstate” pointer (A2+) |
| `src/init.cpp` | **A1:** `InitializeActiveChainstate()` after block index load |
| Call sites | RPCs, wallet, net_processing use **active** chainstate (A2) |
| RPC `getchainstates` | **A1 done:** reports single active chainstate |

**A1 exit criteria (met):** single-chainstate behavior bit-identical; accessors available; no AssumeUTXO yet.  
**A2 exit criteria (met):** high-traffic tip/UTXO reads go through ActiveChain*/ActiveCoinsTip*; tests bind chainstate.  
**A3 exit criteria (met):** dual slots exist; background idle (owns chain, no coins); tip behavior unchanged; RPCs report both.  
**Phase A full exit (met):** dual storage done; further A2-style migrations optional.

### Phase B — Snapshot file format + load

**Format (Dogecoin 1.14 / Phase B1 — per-txid `CCoins`, not Bitcoin’s per-outpoint `Coin`):**

- Magic `utxo\xff` + version `1`  
- Network magic (`MessageStart`)  
- Base block hash  
- Coins count (number of non-pruned `CCoins` records)  
- Repeated: `txid` + `CCoins` serialization  

**Code:**

| Piece | Role |
|-------|------|
| `src/node/utxo_snapshot.h/.cpp` | **B1 done:** metadata + write/load |
| RPC `dumptxoutset` / `loadtxoutset` | **B1 done:** dump active tip; load into background |
| `chainstate_snapshot/` LevelDB | **B1 done:** separate from live `chainstate/` |
| `chainparams` AssumeutxoData / `mapAssumeutxo` | **D1 done:** structure + empty maps (fill when attested hashes published) |
| `activatesnapshot` / `-assumeutxodev` | **B2+D1:** promote tip; attested height+hash skips `-assumeutxodev` |

**B1 exit criteria (met):** dump + load round-trip; background reports `has_snapshot`; active tip unchanged.  
**B2 exit criteria (met):** with `-assumeutxodev` (or regtest), activate snapshot so `chainActive`/`pcoinsTip`/wallet tip = H; IBD tip parked; no Phase C yet.

### Phase C — Background validation

| Piece | Role |
|-------|------|
| Parked IBD coins + chain | **C1:** after B2, background `"ibd"` continues from prior tip toward H |
| `ConnectBlock` into background view | **C1:** does not move wallet tip; uses existing AuxPoW/script rules |
| Expected hash | **C1:** `hash_serialized` frozen at `loadtxoutset` via `ComputeCoinsHashSerialized` |
| Auto step | **C1:** after each `ActivateBestChain` (+ manual `stepbackgroundvalidation`) |
| Progress RPC | **C1:** `getibdinfo` / `getchainstates`: status, height, target, `assumeutxo_validated` |
| Completion | **C1:** match → `assumeutxo_validated=true`; mismatch → fail-closed + shutdown |
| `assumeutxo.dat` | **C2:** base hash, coins hash, status, bg height |
| Startup restore | **C2:** reopen `chainstate_snapshot/`, re-tip `chainActive`/`pcoinsTip` |
| Missing block fetch | **C2:** `GetBackgroundValidationMissingBlocks` + net_processing getdata |
| Collapse | **C2:** on complete set `assumeutxo_dual_collapsed`; stop stepping |

**C1 exit criteria (met):** good snapshot reaches `completed`; bad UTXO hash fail-closes.  
**C2 exit criteria (met):** restart resumes snapshot tip + bg validation; pruned history re-fetched; validated → dual collapsed flag.

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
| IBD parallel download (P0.3) | Done |

## 9. Suggested next engineering

1. ~~Land P0.3 / Phase A–D2~~  
2. Publish community-attested mainnet/testnet entries into `mapAssumeutxo` (use `dumptxoutset` snippet)  
3. Signed snapshot artifacts + prune product rules  
4. Optional: delete on-disk `chainstate/` after collapse; richer GUI modal  

**Attestation workflow:** `dumptxoutset` → `hash_serialized` + `assumeutxo_snippet` → PR into `mapAssumeutxo` → releases accept that height without `-assumeutxodev`.

## 10. Success metric

A laptop with a published mainnet snapshot reaches **wallet-usable tip in &lt; 30 minutes** on a typical home connection, with background validation completing later without user action, and a wrong snapshot **never** producing a silent wrong chain.
