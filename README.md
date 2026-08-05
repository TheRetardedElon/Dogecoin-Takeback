# Dogecoin Takeback — Core Pro

**[TheRetardedElon/Dogecoin-Takeback](https://github.com/TheRetardedElon/Dogecoin-Takeback)**

A Dogecoin Core–based full node and wallet client (**Dogecoin Core Pro**) focused on **native DOGE**: wallet, merchant tools, Meme Stream tips, and a modern shell — **not** an EVM “app layer,” wrapped DOGE, or a second ledger.

Based on Dogecoin Core (Bitcoin Core lineage). Development continues here as a product-oriented client on top of real Dogecoin consensus.

---

## What this is

| Piece | Description |
|--------|-------------|
| **dogecoind / dogecoin-cli / dogecoin-tx** | Full node, RPC, utilities |
| **dogecoin-qt (Core Pro)** | GUI wallet with modern sidebar shell |
| **Doge Business** | Local invoices, POS keypad, payment QR, auto-watch paid |
| **Meme Stream** | Feed / publish / tip creators on-chain (author = wallet address) |
| **Network page** | Peers, bans, disconnect/ban/unban, activity toggle |
| **Arcade** | In-client mini-game tab (pure Qt; no consensus) |
| **Themes** | ThemeManager + Options → Theme (and full Options tabs restored) |
| **IBD / P2P** | `getibdinfo`, stall rescue, ASMAP, parallel block fetch (1.14.101) |
| **AssumeUTXO** | Dual chainstate A–C2: dump/load/activate, background prove, restore (`-assumeutxodev`) |

**Settlement rule:** money that matters is **native DOGE on Dogecoin**. Optional external business platforms (e.g. GPE) are out of scope for this README; this repo ships the Core client.

More product detail: open **`html/docs/index.html`** in a browser (local docs, no server required).

- [Pure DOGE strategy](html/docs/pages/pure-doge-strategy.html)
- [Payment layer](html/docs/pages/payment-layer.html)
- [Core Pro UI reference](html/docs/pages/ui-reference-core-pro.html)
- [IBD & P2P](html/docs/pages/ibd-and-p2p.html)
- [AssumeUTXO](html/docs/pages/assumeutxo.html)
- [Roadmap](html/docs/pages/roadmap.html)
- [Changelog (heavy updates)](DOGECOIN_CHANGELOG.md)

---

## Build (Linux / WSL)

Typical path used in this project (WSL2 Ubuntu):

```bash
# deps: build-essential, libtool, autotools, pkg-config, bsdmainutils,
#       libssl-dev, libevent-dev, libboost-all-dev, libdb5.3++-dev,
#       qtbase5-dev, qttools5-dev, libqrencode-dev, libminiupnpc-dev, ...

./autogen.sh
./configure --with-gui=qt5 --enable-c++17 --with-incompatible-bdb
make -j$(nproc)
# GUI:
make -C src qt/dogecoin-qt -j$(nproc)
```

Binaries (when built):

```text
src/dogecoind
src/dogecoin-cli
src/dogecoin-tx
src/qt/dogecoin-qt
```

See also [BUILD_GUIDE.md](BUILD_GUIDE.md), [html/docs/pages/build-and-run.html](html/docs/pages/build-and-run.html), and upstream-style docs under `doc/`.

Smoke helper (optional):

```bash
./contrib/smoke-core-pro.sh
```

---

## Run

```bash
# Full node + wallet GUI (mainnet)
./src/qt/dogecoin-qt

# Testnet
./src/qt/dogecoin-qt -testnet

# Daemon
./src/dogecoind -daemon
./src/dogecoin-cli getblockchaininfo
```

### Default ports

| Function | mainnet | testnet | regtest |
|----------|--------:|--------:|--------:|
| P2P      |   22556 |   44556 |   18444 |
| RPC      |   22555 |   44555 |   18332 |

Do **not** expose RPC to the public internet.

---

## Product principles

1. **One coin** — Dogecoin L1; no wrap token as the product path.  
2. **Keys in Core** — local Business / POS allocate receive addresses from this wallet.  
3. **Tips are on-chain** — Meme Stream tip uses the creator’s Dogecoin address.  
4. **Payment layer ≠ EVM L2** — merchant UX and optional cloud tools settle real DOGE; this client is not a zk/EVM stack.  
5. **Docs move with code** — living HTML under `html/docs/`.

---

## Repository layout (high level)

```text
src/           Core node, wallet, consensus, RPC
src/qt/        Core Pro GUI (Business, Meme Stream, Network, themes)
html/docs/     Project documentation (open index.html)
contrib/       Scripts and helpers
doc/           Classic Core documentation
depends/       Optional dependency builds
```

---

## Status

- **Branch:** `master` — active integration (expect ongoing UI/product work).  
- Prefer tagged releases for production binaries when tags are published.  
- Pre-release GUI: not for high-value mining or unattended merchant float without your own review.

Upstream Dogecoin resources (reference): [dogecoin/dogecoin](https://github.com/dogecoin/dogecoin).

---

## Contributing

Issues and PRs against this repo. Keep changes aligned with **pure DOGE** settlement (no EVM/wrap product paths in Core Pro).

Please do not commit secrets, RPC passwords, wallet seeds, or private infrastructure notes.

---

## License

Released under the **MIT** license. See [COPYING](COPYING).

Dogecoin branding and heritage remain as in the Dogecoin Core lineage; many source files retain historical Bitcoin Core copyright headers where appropriate.
