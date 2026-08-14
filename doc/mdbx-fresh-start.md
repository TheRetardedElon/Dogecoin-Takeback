# MDBX from a wiped / empty datadir

**Default for a new folder is still LevelDB.** MDBX is the local hot KV we use when you start clean. It is not consensus and not on the wire.

Do **not** convert a live LevelDB `chainstate/` in place. When you are ready, wipe the chain files and start empty.

## Keep

- `wallet.dat` (copy it out first)
- `dogecoin.conf` (we will add `dbengine=mdbx`)
- RPC credentials

## Delete (after File → Exit / a full node stop)

From the datadir (`%APPDATA%\Dogecoin` or your custom folder):

- `chainstate\`
- `blocks\` (includes `blocks\index`)
- `chainstate_snapshot\` and `assumeutxo.dat` if present
- leftover `chainstate_mdbx\` / `blocks\index_mdbx\`

A leftover `ENGINE` stamp with no `CURRENT` / `mdbx.dat` is ignored so a wipe cannot trap you on the old engine.

## Start MDBX

**ImGui (product path)**

1. First-run Welcome: pick **MDBX — new or wiped datadir only**.
2. Or Options → Main → **Next fresh start: MDBX**, then start the node.
3. The GUI writes `dbengine=mdbx` into `dogecoin.conf` and launches with `-dbengine=mdbx`.
4. Fast Sync as usual.

**CLI**

```text
dogecoind -dbengine=mdbx -prune=5500
```

After the first successful open, `chainstate/ENGINE` is `mdbx`. Later starts do not need the flag.

## Do not

- Point `-dbengine=mdbx` at a folder that still has LevelDB `CURRENT`
- Put the live datadir on OneDrive / Drive / iCloud
- Force-kill `dogecoind` during the first flush

## Check

```text
dogecoin-cli getdbengine
dogecoin-cli getblockchaininfo
```

`dbengine` should be `mdbx`.
