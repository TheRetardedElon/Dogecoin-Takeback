# Dogecoin Core Pro (Takeback)

### [github.com/TheRetardedElon/Dogecoin-Takeback](https://github.com/TheRetardedElon/Dogecoin-Takeback)

> **Not a meme-coin wrapper. Not an EVM “DOGE app layer.”**  
> Full **Dogecoin Core (1.14 DNA)** node + wallet — modern Pro shell, Fast Sync, merchant tools — while **consensus stays pure DOGE** (AuxPoW, subsidy, scripts: **untouched**).

<p align="center">
  <img src="https://i.imgur.com/mS6ObYd.png" alt="Dogecoin Core Pro splash" width="420" />
</p>

<p align="center"><b>Dogecoin Core Pro</b> · v1.14.x · same mainnet · better product surface</p>

---

## What you get

| Pillar | What shipped |
|--------|----------------|
| **Identity** | Full Dogecoin rebrand path — Core DNA, DOGE name |
| **Shell** | Modern sidebar: Home · Send · Receive · Transactions · Network · **Doge Business** · **Meme Stream** · **Arcade** · Console |
| **Merchant** | Invoices, POS, QR — keys stay in **this** wallet |
| **Tips** | Meme Stream tips are **on-chain DOGE** to creator addresses |
| **Fast Sync** | Attested UTXO snapshot from HTTPS CDN + fail-closed SHA-256 + background prove |
| **IBD / P2P** | `getibdinfo`, stall rescue, ASMAP, parallel download |
| **AssumeUTXO** | Dual chainstate A→D on **1.14 DNA** (not a Bitcoin-24 paste) |
| **Windows** | PE packages, installers, optional **unique RPC password** per install |

**Settlement rule:** money that matters is **native DOGE on Dogecoin L1.** No wrap token. No second chain for payments.

---

## Screenshots

### Home — balances, tips, Meme Stream rail

<p align="center">
  <img src="https://i.imgur.com/l4aKHH9.png" alt="Core Pro Home with Meme Stream" width="900" />
</p>

### Sync progress (be patient — or use Fast Sync)

<p align="center">
  <img src="https://i.imgur.com/NgHYxwU.png" alt="Wallet sync progress dialog" width="720" />
</p>

Until headers/blocks catch up, balances and recent txs can lag. **Do not spend against incomplete history.**

### Fast Sync from CDN (recommended for new wallets)

**Settings → Fast Sync from CDN…**

<p align="center">
  <img src="https://i.imgur.com/dfKu4oF.png" alt="Settings menu with Fast Sync" width="360" />
  &nbsp;
  <img src="https://i.imgur.com/tMyYTa4.png" alt="Fast Sync dialog" width="520" />
</p>

**What it does (plain English):**

1. Download an **attested UTXO snapshot** from a public HTTPS CDN (default: `sync.doge.gopastearth.com`)  
2. Verify **file SHA-256** fail-closed  
3. Load/activate near height **H** so the wallet can be usable sooner  
4. **Your node** still re-proves history over **P2P in the background**  

CDN = dumb file host. **Not** your ongoing verifier.

| Hash | Meaning |
|------|---------|
| **File SHA-256** | Integrity of the downloaded `.dat` |
| **`hash_serialized`** | Hash of the UTXO set at H (attested in `mapAssumeutxo`) |

**Tips for Fast Sync**

- Prefer a **new/empty datadir** — don’t start a multi‑GB snapshot mid-IBD.  
- Snapshot is multi‑GB; home internet can take a while.  
- You can always **Continue with normal sync (P2P)** instead.  
- Keep the wallet data directory **off** OneDrive/Google Drive/Dropbox.

### Options — prune, Fast Sync preference, themes

<p align="center">
  <img src="https://i.imgur.com/sKdvgSf.png" alt="Options Main — prune and Fast Sync" width="720" />
</p>

**Prune tip:** Dogecoin mainnet wants enough headroom. A target like **~2.8 GB is too tight** and can thrash I/O:

<p align="center">
  <img src="https://i.imgur.com/UX2Xuqt.png" alt="Prune target warning" width="560" />
</p>

Prefer **`-prune=5500`** (≈5.5 GB) or higher for fewer disk spikes. Raising prune later can require **`-reindex`**.

<p align="center">
  <img src="https://i.imgur.com/S91gYwn.png" alt="Theme options" width="720" />
</p>

Themes: Light / Dark / Dogecoin / Neon / Matrix / … plus custom swatches.

### Doge Business — invoices & POS (keys in Core)

<p align="center">
  <img src="https://i.imgur.com/kMrdIFT.png" alt="Doge Business Center invoices" width="900" />
</p>

Create invoices with labels, amounts, QR/URI. Payment detection uses **this wallet** — not an external custodial wallet.

### Arcade — pure client fun

<p align="center">
  <img src="https://i.imgur.com/zzP1dqJ.png" alt="Retr-Doge Shibe Blaster Arcade" width="900" />
</p>

**Retr-Doge Shibe Blaster** — no wallet and no network required. Consensus is not involved.

---

## Binaries

```text
dogecoin-qt    — Core Pro GUI wallet (this product)
dogecoind      — full node + RPC
dogecoin-cli   — control plane
dogecoin-tx    — raw tx toolkit
```

| Port | mainnet | testnet | regtest |
|------|--------:|--------:|--------:|
| P2P | 22556 | 44556 | 18444 |
| RPC | 22555 | 44555 | 18332 |

**Never expose RPC to the public internet.**

### Windows install notes

- Packages under `release/` (e.g. `dogecoin-1.14.102-win64-setup.exe` / zip).  
- Optional **rpcsecure** installer builds generate a **unique `rpcpassword` per install** (no shared default) and write `%APPDATA%\Dogecoin\RPC-CREDENTIALS.txt` when conf is new.  
- Without `rpcpassword` in conf, Core uses **cookie auth** (`.cookie`) for local RPC — also not a fixed default password.

### Headless operators (dump nodes / CDN)

If you run **snapshot producers** or a **Windows Service** dump node, use **[Dogecoin-GPENode](https://github.com/TheRetardedElon/Dogecoin-GPENode)** — same consensus DNA, headless packaging, operator TUI, unique RPC passwords.

---

## Examples

### Run GUI (mainnet)

```bash
./src/qt/dogecoin-qt
# or Windows:
# "C:\Program Files\Dogecoin\dogecoin-qt.exe"
```

### Daemon + CLI

```bash
./src/dogecoind -daemon
./src/dogecoin-cli getblockchaininfo
./src/dogecoin-cli getibdinfo
./src/dogecoin-cli getchainstates
```

### Useful conf snippets

**Safer prune (wallet PC, bounded disk):**

```ini
# dogecoin.conf — example only; tune for your disk
server=1
listen=1
prune=5500
dbcache=1024
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
```

**Prefer Fast Sync path (also set in Options → Main):** GUI checkbox *Prefer Fast Sync when available*.

**Custom snapshot URL** (advanced — you must supply the matching **artifact SHA-256**):

```text
Settings → Options → Main
  Custom snapshot URL:  https://your-cdn.example/utxo-H.dat
  Artifact SHA-256:     <64 hex chars>
```

### AssumeUTXO operator RPCs

| RPC | Job |
|-----|-----|
| `dumptxoutset` | Snapshot UTXO set + `hash_serialized` |
| `loadtxoutset` | Load into background chainstate |
| `activatesnapshot` | Promote snapshot tip (regtest / attested / `-assumeutxodev`) |
| `fetchassumeutxo` | Stream-hash download (HTTPS; WinHTTP on Windows) |
| `listassumeutxo` | Compiled attestation heights |
| `getchainstates` | Dual-state visibility |
| `getibdinfo` | IBD + AssumeUTXO progress |

**Regtest dump/load:**

```bash
dogecoin-cli -regtest dumptxoutset utxo.dat
# fresh node, headers first, then:
dogecoin-cli -regtest loadtxoutset utxo.dat
dogecoin-cli -regtest activatesnapshot
dogecoin-cli -regtest getibdinfo
```

Mainnet activate without attestation requires `-assumeutxodev=1` (**dev only** — not for untrusted snapshots with real funds).

### Windows PE smokes

```powershell
powershell -ExecutionPolicy Bypass -File scripts/smoke-assumeutxo-regtest.ps1
powershell -ExecutionPolicy Bypass -File scripts/smoke-assumeutxo-two-node.ps1
powershell -ExecutionPolicy Bypass -File scripts/smoke-winhttp-https.ps1
```

---

## Architecture (one ledger)

```text
┌─────────────────────────────────────────────────────────────┐
│  dogecoin-qt  Core Pro shell                                │
│  Business · Meme Stream · Network · Arcade · Themes         │
└───────────────────────────┬─────────────────────────────────┘
                            │ RPC / signals
┌───────────────────────────▼─────────────────────────────────┐
│  dogecoind  — validation · wallet · mempool · P2P           │
│  Active chainstate  +  Background / snapshot path           │
│  getibdinfo · AssumeUTXO · ASMAP · parallel download        │
└─────────────────────────────────────────────────────────────┘
              Native DOGE  ·  AuxPoW consensus  ·  one ledger
```

**What we refuse to change:** consensus rules, AuxPoW, subsidy schedule, “make DOGE an EVM chain.”

---

## Version line

| | |
|--|--|
| **Product** | Dogecoin Core Pro |
| **Line** | **1.14 DNA** (Pro + IBD + AssumeUTXO) |
| **Stamp** | **v1.14.102** (Fast Sync CDN path; distinguish from stock 1.14.x) |
| **User-Agent** | Shibetoshi lineage (e.g. `/Shibetoshi:1.15.2/`) |

Pre-release GUI: review builds yourself before large merchant float / mining.

---

## Docs

Open **[`html/docs/index.html`](html/docs/index.html)** offline.

| Page | Topic |
|------|--------|
| [Fast Sync explained](html/docs/pages/fast-sync.html) | CDN bootstrap + trust model |
| [Multi-operator mesh](html/docs/pages/multi-operator-mesh.html) | Many dumpers/mirrors |
| [AssumeUTXO](html/docs/pages/assumeutxo.html) | Dual chainstate A–D |
| [IBD & P2P](html/docs/pages/ibd-and-p2p.html) | Telemetry, rescue, parallelism |
| [UI reference](html/docs/pages/ui-reference-core-pro.html) | Layout contract |
| [Payment layer](html/docs/pages/payment-layer.html) | Invoices / POS / tips |
| [Pure DOGE strategy](html/docs/pages/pure-doge-strategy.html) | Why no wrap / EVM path |
| [Roadmap](html/docs/pages/roadmap.html) | Phases checklist |
| [Changelog](DOGECOIN_CHANGELOG.md) | Full history |

Plan: [`doc/tiered-storage-and-fast-sync.md`](doc/tiered-storage-and-fast-sync.md)

---

## Build

### Linux / WSL

```bash
./autogen.sh
./configure --with-gui=qt5 --enable-c++17 --with-incompatible-bdb
make -j$(nproc)
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

```bash
bash scripts/full-rebuild-dogecoin-qt.sh   # adjust paths for your winbuild tree
```

---

## Product principles

1. **One coin** — Dogecoin L1.  
2. **Keys in Core** — Business / POS / tips use *this* wallet.  
3. **Tips are on-chain** — real DOGE to creator addresses.  
4. **Payment layer ≠ EVM L2** — merchant UX settles L1.  
5. **Measure IBD before magic** — telemetry before snapshot theater.  
6. **Docs move with code** — `html/docs/` stays alive.  
7. **Consensus is sacred** — AuxPoW and rules stay Dogecoin.

---

## Status (honest)

| | |
|--|--|
| **Branch** | `master` — active integration |
| **AssumeUTXO** | A→D engineered; regtest PE smokes green |
| **Mainnet mapAssumeutxo** | e.g. **6324519** attested; more heights as dumps publish |
| **Fast Sync** | CDN + WinHTTP + dialog in `release/dogecoin-1.14.102-win64*` |
| **Mesh** | M1 live (GPE); M2 multi-URL failover next |
| **GUI** | Pre-release — review before high-value float |
| **Upstream DNA** | [dogecoin/dogecoin](https://github.com/dogecoin/dogecoin) lineage |

---

## Contributing

PRs and issues against this repo. Keep changes aligned with **pure DOGE** settlement — no EVM/wrap product paths in Core Pro.

**Do not commit** secrets, RPC passwords, seeds, wallets, or private infra.

---

## License

**MIT** — see [COPYING](COPYING).

Dogecoin branding and heritage remain of the Dogecoin Core lineage.

---

<div align="center">

### Built on real consensus. Shipped with a Pro shell. Aimed at operators who want **native DOGE**.

**Dogecoin Core Pro · 1.14.102 · Takeback**

`getibdinfo` · Fast Sync · Doge Business · Meme Stream · Arcade

</div>
