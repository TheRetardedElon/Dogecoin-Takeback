# Debian package: dogecoin-core-pro

One `.deb`. First install asks **Client / Server / Hybrid** via debconf — the same
question as the Windows NSIS installer.

One `dogecoind`. Never two. Same mainnet consensus as Core Pro / GPENode.

**Testnet:** applications menu **Dogecoin Core Pro Testnet**, or `dogecoin-core-pro --testnet`. That passes `--testnet` into the GUI or TUI (green chrome). It does not start the mainnet systemd unit. Stop mainnet first.

## The apt prompt

```
How are you installing Dogecoin Core Pro?

  Client  - desktop wallet (ImGui). Datadir ~/.dogecoin. No systemd.
  Server  - headless operator (TUI + systemd). Replaces dogecoin-gpenode.
  Hybrid  - both UIs, still one systemd dogecoind.
```

Noninteractive / CI:

```bash
echo "dogecoin-core-pro dogecoin-core-pro/install-role select server" \
  | sudo debconf-set-selections
sudo DEBIAN_FRONTEND=noninteractive apt-get install ./dogecoin-core-pro_*.deb
```

Reconfigure later:

```bash
sudo dpkg-reconfigure dogecoin-core-pro
```

## Paths after install

| Path | Purpose |
|------|---------|
| `/usr/bin/dogecoind` | The one daemon |
| `/usr/bin/dogecoin-cli` | RPC CLI |
| `/usr/bin/dogecoin-core-pro` | Role-aware launcher (`--testnet` supported) |
| `/usr/bin/dogecoin-pro-gui` | ImGui desktop (when packaged) |
| `/usr/bin/gpenode-tui` | Operator TUI (when packaged) |
| `/usr/bin/gpenode-ops` | Operator glue (when packaged) |
| `/etc/dogecoin-core-pro/install-role` | `client` / `server` / `hybrid` |
| `/etc/dogecoin-core-pro/dogecoin.conf` | Node config (Server / Hybrid) |
| `/var/lib/dogecoin-core-pro/` | Datadir (Server / Hybrid) |
| `~/.dogecoin` | Datadir (Client) |
| `/lib/systemd/system/dogecoin-core-pro.service` | systemd unit (enabled for Server / Hybrid only) |

## Build a `.deb` (WSL / Linux)

```bash
cd /mnt/c/dogedev
# Prefer Core Pro Linux daemon at src/dogecoind when present
export VERSION=1.14.105
export ARCH=amd64
bash deploy/debian/build-deb.sh
# Output: release/debian/dogecoin-core-pro_${VERSION}-1_${ARCH}.deb
```

Override binary search:

```bash
export BIN_DIR=/path/to/bin          # dogecoind + dogecoin-cli
export LIB_DIR=/path/to/lib          # optional bundled Boost
bash deploy/debian/build-deb.sh
```

## Transitional note

This package `Provides` / `Replaces` / `Conflicts` `dogecoin-gpenode`.
`apt install dogecoin-gpenode` can later become a metapackage that
installs `dogecoin-core-pro` and preseeds `server`.
