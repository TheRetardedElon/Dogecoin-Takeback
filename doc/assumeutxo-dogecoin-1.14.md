# AssumeUTXO for Dogecoin Core (1.14 DNA) — Design

**Status:** engineering **A–D3 done** · product P1 **~90% shipped** (map + CDN + WinHTTP + FastSyncDialog; see `doc/tiered-storage-and-fast-sync.md`)  
**Depends on:** P0/P0.1 IBD telemetry, ASMAP, healthy single-chainstate  
**Consensus impact:** none if done like Bitcoin (background full validation to the assume height)  
**Related:** tiered storage / CDN / default prune product plan — **P1–P4** in `tiered-storage-and-fast-sync.md` (do not confuse with engineering Phase A)  
**Diagrams:** `html/docs/pages/diagrams.html` · `html/docs/assets/diagrams/assumeutxo-diagram.png`

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
| **P1** Trust anchors + CDN stream-hash + Fast Sync UI | **~90%** | Mainnet `mapAssumeutxo[6324519]`; GPE CDN + WinHTTP; `FastSyncDialog` + first-run; 1.14.102 packages. Open: mesh M2 multi-URL, more heights/mirrors, Linux HTTPS polish |
| **P2** Default prune-as-you-go product | **Partial** | Fast path SoftSet prune; stock `-prune=` works; full default product still open |
| **P3** Optional cold `blk` object CDN | **Open** | secondary fetch; never FUSE |
| **P4** Hot KV engine (RocksDB/…) | **Open** | ops win; orthogonal to dual views |

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
| `chainparams` AssumeutxoData / `mapAssumeutxo` | **D1 done + mainnet filled:** structure + **height 6324519** attested; more heights as dumps publish |
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

### Phase D — Product hooks (engineering)

- [x] GUI status-bar historical validation % (D1)  
- [x] Attestation gate structure `mapAssumeutxo` / `AssumeutxoData` (D1–D2)  
- [x] Prune interaction during bg proof (D3)  
- [x] **Published** mainnet map entry height **6324519** (more heights / testnet still open)  
- [x] Stream-and-hash CDN download + first-run Fast Sync modal → **Product P1 ~90%**  
- [ ] Default prune product path → **Product P2** (partial SoftSet only)  

Full product architecture (CDN as dumb pipe, two-hash trust model, diagrams):  
**`doc/tiered-storage-and-fast-sync.md`** · **`html/docs/pages/diagrams.html`**.

### Trust: two hashes (do not confuse)

| Field | Meaning |
|-------|---------|
| `AssumeutxoData.hash_serialized` | Hash of the **UTXO set** at H (coins), same family as `gettxoutsetinfo` |
| Artifact digest (P1) | Hash of the **snapshot file bytes** from the CDN (integrity in transit) |

Gemini-style “`hash_serialized` = SHA-256 of the file” is **wrong**. Both checks are required for Fast Sync.

## 5. Dogecoin-specific checklist

- [x] AuxPoW blocks validate in background path via existing `CheckAuxPowProofOfWork` (uses normal ConnectBlock)  
- [ ] Digishield / subsidy epochs correct across chosen H (verify when picking H)  
- [x] Pruned nodes: tip can run without full history; bg proof needs blocks (fetch / refuse prune mid-proof)  
- [ ] 1-minute block spacing → choose H with stable multi-party attestation  
- [ ] Snapshot size estimate & hosting (CDN; multi‑GB, minutes of I/O)  
- [x] No change to AuxPoW chain ID, merge-mining magic, or consensus constants  

## 6. File surface estimate (1.14)

**Shipped (A–D3):**  
`node/chainstate.*`, `node/utxo_snapshot.*`, `validation` dual paths, `rpc/blockchain` dump/load/list, `chainparams` map hooks, Qt status progress  

**Product P1 shipped (~90%):**  
stream-hash downloader, manifest, GUI first-run modal, mainnet `mapAssumeutxo[6324519]`, GPE CDN hosting, WinHTTP Windows packages  

**Product still open after P1 package:**  
mesh M2 multi-URL failover, more attested heights / independent mirrors, Linux HTTPS polish, P2 default prune product  

**Untouched (goal):**  
`auxpow.*`, `pow.*` subsidy, script interpreter consensus  

## 7. What engineering A–D did **not** include

- RocksDB migration (Product **P4**)  
- ~~HTTP CDN snapshot fetch (Product **P1**)~~ → **done** (WinHTTP + stream-hash)  
- Default prune installer policy (Product **P2**)  
- Cold `blk` object CDN (Product **P3**)  
- Multi-operator mesh M2+ client failover  
- Utreexo / ZK proofs  
- Soft-forking a consensus-committed UTXO root  

## 8. Prerequisites already done in this repo

| Prerequisite | Status |
|--------------|--------|
| IBD telemetry (`getibdinfo`) | Done |
| Peer stall + rescue | Done |
| IBD flush policy | Done |
| ASMAP peer diversity | Done |
| IBD parallel download (P0.3) | Done |
| AssumeUTXO dual path A–D3 | Done |
| Fast Sync product P1 (map + CDN + dialog) | ~90% shipped |

## 9. Suggested next engineering (product)

1. ~~Land P0.3 / engineering Phase A–D3~~  
2. ~~**Product P1:** stream-and-hash + GUI Fast Sync + mainnet map entry~~  
3. **Mesh M2:** multi-URL manifest + client failover; second independent mirror  
4. More multi-party dumps → additional `mapAssumeutxo` heights  
5. **Product P2:** default prune for wallet-node installs  
6. **P3/P4** as capacity allows  

**Attestation workflow:** `dumptxoutset` → `hash_serialized` + `assumeutxo_snippet` → PR into `mapAssumeutxo` → host artifact + **artifact digest** in manifest → releases accept that height without `-assumeutxodev`.

## 10. Success metric

A laptop with a published mainnet snapshot reaches **wallet-usable tip in tens of minutes** on a typical home connection (honest: multi‑GB download + disk deserialize — not “30 seconds”), with background validation completing later without user action, and a wrong snapshot **never** producing a silent wrong chain. Disk for non-archival users stays **bounded** via prune defaults (P2).
