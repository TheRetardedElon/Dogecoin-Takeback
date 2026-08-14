# Install roles — Client / Server / Hybrid

**One `dogecoind`. Never two.** ImGui and the operator TUI are control planes over localhost RPC.

This is how Core Pro absorbs [Dogecoin-GPENode](https://github.com/TheRetardedElon/Dogecoin-GPENode) without a second consensus binary or a second datadir.

## Testnet

Same `dogecoind`, different chain. Datadir `%APPDATA%\\Dogecoin\\testnet3` (or `~/.dogecoin/testnet3`). RPC **44555**, P2P **44556**. Testnet coins are worthless.

| Start from | Command / shortcut |
|---|---|
| Windows Start Menu / desktop | **Dogecoin Core Pro Testnet** (green coin icon) → `corepro-launch.exe --testnet` |
| Server Start Menu | **Dogecoin Operator TUI Testnet** → `gpenode-tui.exe --testnet` |
| Linux menu / CLI | **Dogecoin Core Pro Testnet** / `dogecoin-core-pro --testnet` |
| ImGui | Options → Main → Testnet, or `dogecoin-pro-gui.exe --ui gfx --testnet` |
| TUI | `gpenode-tui --testnet` (green chrome + TESTNET header) |
| CLI | `dogecoin-cli -testnet …` (green banner on a TTY) |
| Daemon | `dogecoind -testnet -server` |

- ImGui defaults to the **Matrix** (green) theme. The operator TUI swaps gold for green.
- Stop the mainnet node first (one `dogecoind` process). The Hybrid Windows service / systemd unit is mainnet-only; testnet starts a separate process and will not start or stop that service.
- Fast Sync CDN is mainnet-only. Testnet IBD is from testnet peers.
- HTML: [`html/docs/pages/testnet.html`](../html/docs/pages/testnet.html)

## The question (every OS installer)

> How are you installing this?

| Role | What they get | Who |
|---|---|---|
| **Client** | ImGui desktop (`dogecoin-pro-gui`) + one `dogecoind` | Daily wallet / Fast Sync users |
| **Server** | Operator TUI (`gpenode-tui`) + `gpenode-ops` + one `dogecoind` (service / systemd). No ImGui. | Dump nodes, VPS, apt `dogecoin-gpenode` replacement |
| **Hybrid** | Both UIs + one `dogecoind`. Launcher asks: desktop GUI or operator TUI. | People who switch |

Installer size is roughly **max(client, server)** plus the other UI. The node is not duplicated.

## Process model

```
                    ┌─────────────────────────┐
                    │  dogecoind (exactly 1)  │
                    │  datadir / wallet / P2P │
                    └───────────┬─────────────┘
                                │ 127.0.0.1 JSON-RPC
              ┌─────────────────┼─────────────────┐
              ▼                 ▼                 ▼
     dogecoin-pro-gui     gpenode-tui        dogecoin-cli
        (ImGui)           (operator TUI)     (scripts)
```

| Rule | Why |
|---|---|
| One datadir | Two nodes on one chainstate corrupt BDB / LevelDB |
| One RPC cookie or user/pass | Both UIs use the same `dogecoin.conf` |
| UIs never run `AppInit` | Consensus stays in C++ |
| Hybrid tray does not start a second daemon | The GUI owns the one notify icon; it only opens a UI |

## What each role starts

| Role | Starts `dogecoind` via | Opens by default |
|---|---|---|
| Client | Pro GUI / `dogeinit` (Path A) | ImGui |
| Server | Windows Service `gpenode-ops service-run` or systemd `dogecoin-gpenode` | TUI |
| Hybrid | **One** supervisor: Windows service `DogecoinGPENode` (display name Dogecoin Core Pro) via `gpenode-ops service-run`, or systemd `dogecoin-core-pro` on Linux. Same datadir as the GUI. Tray/launcher: **Desktop GUI** or **Operator TUI**. | Ask |

Hybrid means this PC is a **full node (a server)** plus a desktop wallet — not a light client. Window **X** / Hide / Minimize to tray must **not** stop `dogecoind`. There is **one** tray icon (the Desktop GUI). File → **Exit** and that icon's **Quit and stop node** RPC-stop `dogecoind` first (flush chainstate / block index), wait for the process to exit, **then** stop the Windows service (`DogecoinGPENode`) so SCM failure-restart cannot bring it back. Same Exit path on Client. Do not force-kill during that splash — a kill mid-flush can drop the MDBX tip and IBD looks like it started over. If the process is up but RPC never comes back, the GUI restarts `DogecoinGPENode` once. The GUI auto-reads `rpcuser`/`rpcpassword` from the installer `dogecoin.conf`. Do not also launch `gpenode-tray` — that was a second icon whose Exit left the node running.

IBD already resumes from `blk*.dat` after a **clean** stop (blocks are written as they arrive). The node now also writes the block index every two minutes during IBD so a crash does not require a full re-download. The 0.0% bar is progress vs the **whole** chain (millions of blocks), not vs the current header count.

Hybrid **must not** let ImGui spawn a second `dogecoind` if the service already owns it. Pro GUI already treats “dogecoind already running” as success.

**Hybrid open-first (every install, every machine):** default is `ask`. Closing the app and opening **Dogecoin Core Pro** shows the picker again unless they checked Remember. Direct “Desktop GUI” Start shortcuts are not shipped (Windows Search was skipping the picker). The GUI/TUI bounce back to the picker when Hybrid + `ask` and they were not launched with `--ui gfx|tui`.

Stored in `hybrid-ui.txt` (Windows) or `/etc/dogecoin-core-pro/hybrid-ui` (Linux): `ask` / `gfx` / `tui`. Change it in:

- the Hybrid picker (“Remember this”)
- ImGui **Options → Hybrid**
- TUI **Settings → H** (or command `hybrid ask|gfx|tui`)

Qt (`dogecoin-qt`) is **not** shipped. Windows Start was indexing leftover `dogecoin-qt.exe` as “Dogecoin Core”.

Uninstall always **stops** `dogecoind` (service + RPC stop + wait) before deleting program files. Optional **FULL PURGE** deletes the datadir (blocks, chainstate, `wallet.dat`). Leaving purge unchecked can leave tens of GB on disk.

## Conf profiles (`write-install-conf.ps1`)

| Role | `-Profile` | Notes |
|---|---|---|
| Client | `core` | Wallet on, localhost RPC |
| Server | `dump` | `disablewallet=1`, `prune=5500` (operator dump default) |
| Hybrid | `hybrid` | Wallet on, localhost RPC, same single datadir |

`install-role.txt` is written next to the binaries so tray / launchers know the mode.

## Apt (same three roles)

`apt install ./dogecoin-core-pro_*.deb` runs **debconf** with the same question.
See [deploy/debian/README.md](../deploy/debian/README.md).

| Package | Contents |
|---|---|
| `dogecoin-core-pro` | One `.deb`: node + role prompt. Client skips systemd. Server/Hybrid enable `dogecoin-core-pro.service`. |

Later we can split into `-node` / `-client` / `-server` metapackages if apt size matters.
`dogecoin-gpenode` is a transitional Provides/Replaces of this package.

Noninteractive:

```bash
echo "dogecoin-core-pro dogecoin-core-pro/install-role select server" | sudo debconf-set-selections
sudo DEBIAN_FRONTEND=noninteractive apt-get install ./dogecoin-core-pro_*.deb
```

## Fresh MDBX (when you are ready to wipe the chain)

See [mdbx-fresh-start.md](mdbx-fresh-start.md). Keep `wallet.dat`. Delete `chainstate/` and `blocks/`. First-run or `dbengine=mdbx`. Do not flip a live LevelDB folder.

## What we will not do

- Install two `dogecoind` copies that both run
- Point TUI and ImGui at different datadirs by default
- Rewrite consensus in Go
- Put live `chainstate` / `wallet.dat` on OneDrive
