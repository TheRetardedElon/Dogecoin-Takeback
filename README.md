# Dogecoin Core Pro (Takeback)

### [github.com/TheRetardedElon/Dogecoin-Takeback](https://github.com/TheRetardedElon/Dogecoin-Takeback)

> **Not a wrapper. Not an EVM “DOGE app layer.” Not Qt.**  
> Full **Dogecoin Core (1.14 DNA)** node + wallet. Consensus (AuxPoW, subsidy, scripts) is **untouched**.  
> Latest release: **[v1.14.104](https://github.com/TheRetardedElon/Dogecoin-Takeback/releases/tag/v1.14.104)**

<p align="center">
  <img src="https://i.imgur.com/OJMui2T.png" alt="Dogecoin Core Pro — Home" width="860" />
</p>

<p align="center"><b>Dogecoin Core Pro</b> · v1.14.104 · ImGui desktop · Client / Server / Hybrid</p>

---

## What this is

You run **one** `dogecoind`. It speaks the same mainnet as official Core. Other wallets and miners do not need our GUI.

The desktop is **ImGui** (`dogecoin-pro-gui`). `dogecoin-qt` is **not shipped**.

```text
                    ┌─────────────────────────────────┐
                    │  dogecoind  (exactly one)       │
                    │  consensus · P2P · wallet.dat   │
                    │  chainstate + blocks            │
                    └───────────────┬─────────────────┘
                                    │ 127.0.0.1 JSON-RPC
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
     dogecoin-pro-gui         gpenode-tui            dogecoin-cli
      ImGui desktop          operator TUI              scripts
              ▲
              │
        corepro-launch   (Windows: no console — Hybrid picker)
        gpenode-tray     (system tray: reopen GUI or TUI)
```

| Piece | What it is | What it is not |
|-------|------------|----------------|
| **`dogecoind`** | The node. Validates blocks like everyone else. | Not a second coin. |
| **ImGui GUI** | Wallet / Fast Sync / Arcade over localhost RPC | Does not run consensus on the render thread |
| **Operator TUI** | Server / Hybrid ops | Does not rewrite consensus in Go |
| **`corepro-launch`** | Native Windows launcher + Hybrid picker | Not a PowerShell/cmd window |
| **CDN** (`sync.doge.gopastearth.com`) | Static HTTPS files for Fast Sync | Not your live database. Not RPC. |

**Two nodes on one datadir corrupt the databases.** Installers never do that.

---

## Install

Download **[v1.14.104](https://github.com/TheRetardedElon/Dogecoin-Takeback/releases/tag/v1.14.104)**.

| File | Who |
|------|-----|
| `dogecoin-1.14.104-win64-setup-rpcsecure.exe` | Windows |
| `dogecoin-core-pro_1.14.104-1_amd64.deb` | Debian / Ubuntu (or apt, below) |

The installer asks **how this machine is used** (same question on Linux apt):

| Role | You get | Datadir | Node start |
|------|---------|---------|------------|
| **Client** | ImGui desktop | `%APPDATA%\Dogecoin` / `~/.dogecoin` | GUI starts `dogecoind` if needed |
| **Server** | TUI + service | Linux: `/var/lib/dogecoin-core-pro` | Windows service / systemd |
| **Hybrid** | Both UIs, **one** node | Same as Client on desktop | Service + native picker |

**Hybrid:** Start Menu **Dogecoin Core Pro** opens `corepro-launch.exe` (no black console). Default is **ask** which UI — Desktop GUI or Operator TUI — unless you check Remember. Change later in **Options → Hybrid** or TUI **Settings → H**.

Each new install gets a **unique RPC password** on `127.0.0.1` only. See `RPC-CREDENTIALS.txt` in the datadir.

**Close / tray:** window **X** sends the UI to the system tray. The node is **not** closed. File → **Exit** is the sanitary shutdown (Client stops the node after flush; Hybrid can leave it or **Stop node and exit**). Hybrid tray can reopen Desktop GUI or Operator TUI.

Operators: same packages on [Dogecoin-GPENode v1.14.104-gpenode](https://github.com/TheRetardedElon/Dogecoin-GPENode/releases/tag/v1.14.104-gpenode).

Linux apt:

```bash
curl -fsSL https://apt.dogecli.gopastearth.com/pubkey.gpg \
  | sudo gpg --dearmor -o /usr/share/keyrings/gpenode.gpg
echo "deb [signed-by=/usr/share/keyrings/gpenode.gpg] https://apt.dogecli.gopastearth.com stable main" \
  | sudo tee /etc/apt/sources.list.d/gpenode.list
sudo apt update
sudo apt install dogecoin-core-pro
# or: sudo apt install dogecoin-gpenode   # transitional — pulls Core Pro
```

Full role spec: [`doc/install-roles.md`](doc/install-roles.md)

---

## Screenshots

### First run — role, datadir, boot

<p align="center">
  <img src="https://i.imgur.com/btEJ7k3.png" alt="Install role — Client, Server, or Hybrid" width="720" />
</p>

<p align="center">
  <img src="https://i.imgur.com/hZlTfxs.png" alt="First-run wallet and datadir setup" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/Ai3cjnR.png" alt="Boot splash — node init checklist" width="860" />
</p>

### Desktop

<p align="center">
  <img src="https://i.imgur.com/OJMui2T.png" alt="Home" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/BxhQQkK.png" alt="Send and Receive" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/LDo6q8y.png" alt="Transactions" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/ZKNk2au.png" alt="Blockchain" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/2RNDnPR.png" alt="Network map and peers" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/Hv44GE6.png" alt="Mining" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/p9WJzSu.png" alt="Arcade" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/5l445C6.png" alt="RPC console" width="860" />
</p>

### Tray

<p align="center">
  <img src="https://i.imgur.com/XTxvrRx.png" alt="X sends Core Pro to the system tray — node stays running" width="720" />
</p>

### Options

<p align="center">
  <img src="https://i.imgur.com/NTnHi1V.png" alt="Options — Main" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/r2RfjOs.png" alt="Options — Hybrid" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/Q6uKZ7e.png" alt="Options — Wallet" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/u3TyZuc.png" alt="Options — Network" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/GVELbFS.png" alt="Options — Connections" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/0t9zHwU.png" alt="Options — Window" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/Kj0OtHW.png" alt="Options — Display" width="860" />
</p>

<p align="center">
  <img src="https://i.imgur.com/7Hnolax.png" alt="Options — Theme" width="860" />
</p>

---

## Storage (honest)

Most of a full node’s disk is `blocks/blk*.dat` (raw blocks), not the UTXO database.

| Data | Engine today | Notes |
|------|----------------|-------|
| UTXO / block index | **LevelDB default** | Same class of store other Core nodes use. |
| Optional hot engine | **MDBX** (`-dbengine=mdbx`) | Empty dir only, or `-migratedb=mdbx` then exit. Do not flip a live old chainstate. |
| Wallet keys | **Berkeley DB** `wallet.dat` | Unchanged. |
| Raw blocks | Flat files | `prune=5500` is the real size win. |
| Pruned history | Optional **archive** | `-archivepath=` copies finalized `blk`/`rev` before delete. |

**Cloud is a pipe, not a live database.**

- Fast Sync CDN = download a snapshot file, SHA-256 fail-closed, then validate locally.
- Consumer clouds (OneDrive, Drive, iCloud, Dropbox, …) are **refused as the live datadir**.
- Operator file stores (`/mnt/vfs`, NFS) are OK for **archive / snapshot dest only**.

---

## Fast Sync

Download an attested UTXO snapshot from HTTPS → verify file SHA-256 → load near height H → **your node still re-proves history over P2P**.

Default CDN: `https://sync.doge.gopastearth.com/latest.json`

| Hash | Meaning |
|------|---------|
| File SHA-256 | The `.dat` was not truncated or swapped |
| `hash_serialized` | UTXO set at H (compiled in `mapAssumeutxo`) |

Prefer an **empty datadir**. Do not Fast Sync a folder that is already mid-IBD + prune. You can always sync from peers instead.

---

## Binaries

```text
dogecoind           — the node (consensus, wallet DB, P2P, RPC)
dogecoin-cli        — RPC client
dogecoin-tx         — raw tx toolkit
dogecoin-pro-gui    — ImGui desktop
corepro-launch      — Windows launcher / Hybrid picker (no console)
gpenode-tui         — operator TUI (Server / Hybrid)
gpenode-ops         — service host + operator glue (not consensus)
gpenode-tray        — system tray
```

| Port | mainnet | testnet | regtest |
|------|--------:|--------:|--------:|
| P2P | 22556 | 44556 | 18444 |
| RPC | 22555 | 44555 | 18332 |

**Never expose RPC to the public internet.**

---

## What we will not change

Consensus rules, AuxPoW, subsidy schedule, “make DOGE an EVM chain.”  
Settlement that matters is **native DOGE on Dogecoin L1.**

---

## Docs

Open **[`html/docs/index.html`](html/docs/index.html)** offline.

| Page | Topic |
|------|--------|
| [How it works](html/docs/pages/how-it-works.html) | Ecosystem map |
| [Install roles](html/docs/pages/install-roles.html) | Client / Server / Hybrid |
| [Diagrams](html/docs/pages/diagrams.html) | Topology |
| [Storage stack](html/docs/pages/storage-stack.html) | LevelDB default, MDBX opt-in, cloud rules |
| [GPENode](html/docs/pages/gpenode.html) | Operators / dumps |

Also: [`doc/install-roles.md`](doc/install-roles.md) · [`doc/qt-phase-out.md`](doc/qt-phase-out.md) · [`doc/startup-performance.md`](doc/startup-performance.md)

---

## Build (developers)

Desktop users should use the **release installer**, not this.

```bash
./autogen.sh
./configure --without-gui --enable-c++17 --with-incompatible-bdb
make -j$(nproc)
```

ImGui GUI + launcher: `pro-gui/` (CMake + GLFW).

---

## License

[MIT](COPYING) — Dogecoin Core / Bitcoin Core lineage.
