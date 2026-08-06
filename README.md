# Ð Dogecoin Takeback — **Core Pro**

### [github.com/TheRetardedElon/Dogecoin-Takeback](https://github.com/TheRetardedElon/Dogecoin-Takeback)

> **Not a meme coin wrapper. Not an EVM “DOGE app layer.”**  
> A full **Dogecoin Core (1.14 DNA)** node + wallet — rebranded, re-shelled, productized, and upgraded for real operators — while **consensus stays pure DOGE** (AuxPoW, subsidy, scripts: **untouched**).

If you open this repo expecting another “to the moon” landing page: wrong door.  
If you open it expecting **a client that actually ships full-node infrastructure + merchant UX on native DOGE**: **welcome.**

---

## Holyyyyy — what did you *do* to Dogecoin Core?

We took Dogecoin Core’s **1.14 line** and turned it into **Dogecoin Core Pro**: identity, product surface, IBD/P2P muscle, and a full **AssumeUTXO dual-chainstate pipeline** — without inventing a second ledger.

| Pillar | What landed |
|--------|-------------|
| **Identity** | Full Dogecoin rebrand path: binaries, Qt, locales, consensus package names, RPC strings — Core DNA, DOGE name |
| **Shell** | Modern **sidebar Pro UI** (Home / Send / Receive / History / **Network** / **Doge Business** / **Meme Stream** / **Arcade** / Console) |
| **Merchant** | **Doge Business**: invoices, POS keypad, QR, auto-mark paid when the wallet receives |
| **Social tips** | **Meme Stream**: feed / publish / like; **tips are on-chain** to the creator’s Dogecoin address |
| **Network UX** | Live network page + world peer map (lazy-loaded) + full debug console parity |
| **IBD / P2P** | `getibdinfo`, stall rescue, flush policy, ASMAP, parallel block download — **1.14.101** |
| **AssumeUTXO** | Dual chainstate **A→D**: dump / load / activate / background prove / persist / attestation hooks / prune guard / smokes |
| **Docs** | Living HTML docs under `html/docs/` — open `index.html` offline |
| **Windows** | Full PE build story, smoke scripts, crash hardening for Pro shell |

**Settlement rule (non-negotiable):** money that matters is **native DOGE on Dogecoin L1.**  
No wrap token. No “pro version of DOGE” ERC-20. No second chain for payments.

---

## Feature map — open the box

### Full node (still the real thing)

```text
dogecoind      — full node + RPC
dogecoin-cli   — control plane
dogecoin-tx    — raw tx toolkit
dogecoin-qt    — Core Pro GUI wallet
```

Same job as Core: validate, relay, wallet, mine (if you want).  
Different experience: **Pro product surface + serious IBD/AssumeUTXO tooling.**

### Core Pro GUI (the “wait what” UI)

- **Modern nav shell** — dark Pro chrome, not 2013 Bitcoin Qt cosplay  
- **Home** — overview + Meme Stream rail  
- **Doge Business** — dashboard / invoices / POS (keys stay in *this* wallet)  
- **Meme Stream** — full page + media; tip → creator DOGE address  
- **Network** — connections, headers/blocks, IBD telemetry, peer map  
- **Arcade** — Retr-Doge mini-game tab (pure Qt — zero consensus)  
- **Themes** — ThemeManager + Options → Theme (Preview / custom swatches / Pro shell CSS)  
- **Options** — Main / Wallet / Network / Window / Display / Theme restored  
- **Debug window** — Information / Console / Traffic / Peers  

### IBD & P2P (1.14.101 — measure, unstick, parallelize)

Because multi-hour sync without telemetry is flying blind:

| Capability | Why it matters |
|------------|----------------|
| **`getibdinfo` + `IBDStats`** | Flush / stall / rescue / connect timing in one RPC |
| **Stall rescue (`-ibdrescue`)** | Don’t die forever on a stuck peer |
| **Flush policy + prune tips** | Fewer thrash flushes mid-IBD |
| **ASMAP (`-asmap`)** | ASN-aware peer groups |
| **Parallel IBD download** | More blocks in flight / more header peers |
| **Header vs block progress UI** | Status + modal finally agree during “Syncing Headers…” |

### AssumeUTXO on **1.14 DNA** (this is the big one)

Not a Bitcoin Core 24+ paste. **Dogecoin-shaped** dual chainstate:

```text
Phase A  Dual chainstate foundation (active + background)
Phase B  dumptxoutset / loadtxoutset / activatesnapshot
Phase C  Background ConnectBlock + hash fail-closed + persist/restore
Phase D  Attestation map hooks + GUI status + prune guard + tests
```

**Operator RPCs**

| RPC | Job |
|-----|-----|
| `dumptxoutset` | Snapshot UTXO set + `hash_serialized` + **chainparams snippet** |
| `loadtxoutset` | Load into background (`activate` optional) |
| `activatesnapshot` | Promote snapshot tip (regtest / attested / `-assumeutxodev`) |
| `listassumeutxo` | Compiled attestation heights |
| `getchainstates` | Dual-state visibility |
| `getibdinfo` | IBD + AssumeUTXO progress / collapse flags |
| `stepbackgroundvalidation` | Manual proof steps |

**Proven on Windows PE (regtest)**

```powershell
# Same machine — full dump → load → activate → collapse
powershell -ExecutionPolicy Bypass -File scripts/smoke-assumeutxo-regtest.ps1

# Producer dumps tip; consumer loads snapshot + proves history over P2P
powershell -ExecutionPolicy Bypass -File scripts/smoke-assumeutxo-two-node.ps1
```

Also: `qa/rpc-tests/assumeutxo.py` (native Linux `dogecoind` with AssumeUTXO built-in).

> **Honest status:** mainnet/testnet `mapAssumeutxo` entries are **empty until community-attested heights + hashes are published**.  
> The **pipeline is real**. Official public snapshot trust is the next ops chapter — not vaporware code.

---

## Architecture snapshot

```text
┌─────────────────────────────────────────────────────────────┐
│  dogecoin-qt  Core Pro shell                                │
│  Business · Meme Stream · Network · Arcade · Themes         │
└───────────────────────────┬─────────────────────────────────┘
                            │ RPC / signals
┌───────────────────────────▼─────────────────────────────────┐
│  dogecoind  — validation · wallet · mempool · P2P           │
│  ┌─────────────────────┐   ┌──────────────────────────────┐ │
│  │ Active chainstate   │   │ Background / snapshot path   │ │
│  │ (IBD or snapshot)   │   │ load → prove → collapse      │ │
│  └─────────────────────┘   └──────────────────────────────┘ │
│  getibdinfo · IBDStats · ASMAP · parallel download          │
└─────────────────────────────────────────────────────────────┘
                            │
              Native DOGE  ·  AuxPoW consensus  ·  one ledger
```

**What we refuse to change:** consensus rules, AuxPoW, subsidy schedule, “just make DOGE an EVM chain” nonsense.

---

## Version line

| | |
|--|--|
| **Product** | Dogecoin Core Pro |
| **Line** | **1.14 DNA** (Pro + IBD + AssumeUTXO program) |
| **Stamp** | **v1.14.101** (distinguish from stock 1.14.x packages) |
| **User-Agent** | Shibetoshi lineage |

Pre-release GUI: **use at your own risk** for large merchant float / mining until you’ve reviewed builds yourself.

---

## Docs that actually exist

Open **[`html/docs/index.html`](html/docs/index.html)** in a browser (no server).

| Page | |
|------|--|
| [Pure DOGE strategy](html/docs/pages/pure-doge-strategy.html) | Why no wrap / EVM product path |
| [Payment layer](html/docs/pages/payment-layer.html) | Invoices / POS / tips on L1 |
| [UI reference](html/docs/pages/ui-reference-core-pro.html) | Screenshots + layout contract |
| [IBD & P2P](html/docs/pages/ibd-and-p2p.html) | Telemetry, rescue, parallelism |
| [AssumeUTXO](html/docs/pages/assumeutxo.html) | Dual chainstate + smokes |
| [Roadmap](html/docs/pages/roadmap.html) | Phases 0–7 checklist |
| [Architecture](html/docs/pages/architecture.html) | System view |
| [Changelog (heavy)](DOGECOIN_CHANGELOG.md) | Full war diary |

Design notes: `doc/assumeutxo-dogecoin-1.14.md`, `doc/ibd-p0-peer-telemetry.md`.

---

## Build

### Linux / WSL (native)

```bash
./autogen.sh
./configure --with-gui=qt5 --enable-c++17 --with-incompatible-bdb
make -j$(nproc)
# GUI:
make -C src qt/dogecoin-qt -j$(nproc)
```

```text
src/dogecoind
src/dogecoin-cli
src/dogecoin-tx
src/qt/dogecoin-qt
```

See [BUILD_GUIDE.md](BUILD_GUIDE.md) and [html/docs/pages/build-and-run.html](html/docs/pages/build-and-run.html).

### Windows PE (cross from WSL)

This tree carries a **real Windows cross-build story** (depends + mingw). Prefer a **full Qt rebuild** after GUI work:

```bash
# From WSL, example helper (adjust paths to your winbuild tree):
bash scripts/full-rebuild-dogecoin-qt.sh
```

Other helpers under `scripts/` (relink, theme, progress UI, AssumeUTXO smokes).

---

## Run

```bash
# Mainnet GUI
./src/qt/dogecoin-qt

# Daemon
./src/dogecoind -daemon
./src/dogecoin-cli getblockchaininfo
./src/dogecoin-cli getibdinfo
./src/dogecoin-cli getchainstates
```

| | mainnet | testnet | regtest |
|--|--------:|--------:|--------:|
| P2P | 22556 | 44556 | 18444 |
| RPC | 22555 | 44555 | 18332 |

**Do not expose RPC to the public internet.**

### AssumeUTXO (regtest / dev)

```bash
# Dump tip coins
dogecoin-cli -regtest dumptxoutset utxo.dat

# Fresh consumer: headers first, then
dogecoin-cli loadtxoutset utxo.dat
dogecoin-cli activatesnapshot   # regtest always allowed
dogecoin-cli getibdinfo         # snapshot_active, assumeutxo_progress, collapse
```

Mainnet activate without attestation requires `-assumeutxodev=1` (dev only — **not** for untrusted snapshots with real funds).

---

## Product principles

1. **One coin** — Dogecoin L1.  
2. **Keys in Core** — Business / POS / tips use *this* wallet.  
3. **Tips are on-chain** — creator address, real DOGE.  
4. **Payment layer ≠ EVM L2** — merchant UX settles L1.  
5. **Measure IBD before magic** — telemetry and peer path before snapshot theater.  
6. **Docs move with code** — `html/docs/` stays alive.  
7. **Consensus is sacred** — AuxPoW and rules stay Dogecoin.

---

## Repo layout

```text
src/              Node, wallet, consensus, RPC, dual chainstate
src/qt/           Core Pro GUI (Business, Meme Stream, Network, Arcade, themes)
src/node/         AssumeUTXO chainstate + snapshot plumbing
qa/rpc-tests/     assumeutxo.py + classic Core tests
scripts/          Windows rebuild + AssumeUTXO PE smokes
html/docs/        Living product docs (open index.html)
doc/              Design notes + classic Core docs
DOGECOIN_CHANGELOG.md   Heavy-update history
```

---

## Status (read this before you YOLO)

| | |
|--|--|
| **Branch** | `master` — active integration |
| **AssumeUTXO code** | A→D engineered; regtest **1-node + 2-node** PE smokes green |
| **Mainnet mapAssumeutxo** | Empty until attested heights published |
| **GUI** | Pre-release — review before high-value merchant float |
| **Upstream DNA** | Dogecoin Core / Bitcoin Core lineage |

Upstream reference: [dogecoin/dogecoin](https://github.com/dogecoin/dogecoin).

---

## Contributing

PRs and issues against this repo. Keep changes aligned with **pure DOGE** settlement — no EVM/wrap product paths in Core Pro.

**Do not commit** secrets, RPC passwords, seeds, wallets, or private infra.

---

## License

**MIT** — see [COPYING](COPYING).

Dogecoin branding and heritage remain of the Dogecoin Core lineage. Many source files retain historical Bitcoin Core copyright headers where appropriate.

---

<div align="center">

### Built on real consensus. Shipped with a Pro shell. Aimed at operators who want **native DOGE** — not a costume.

**Dogecoin Core Pro · 1.14.101 · Takeback**

`getibdinfo` · `getchainstates` · `dumptxoutset` · `activatesnapshot`

</div>
