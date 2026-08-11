# Dogecoin Core Pro 1.14.103 — Fast Sync preflight + coins_count fix

## Fixes
- **Fast Sync:** snapshot `coins_count` sanity cap raised (100M → 500M). Fixes false "looks unreasonable" on live mainnet dumps (~182M coins).
- **Fast Sync preflight** (before multi-GB download):
  1. Resolve manifest / custom URL
  2. Height must be in `mapAssumeutxo` (unless `-assumeutxodev`)
  3. Manifest `hash_serialized` must match chainparams for that height
  4. Free disk >= snapshot size + 2 GiB margin
  5. HTTP(S) probe of artifact URL (Windows HEAD/Range)
- Attested height **6325931** added (current GPE CDN latest.json).
- Version stamp **1.14.103**

## Headless / GPENode
- Shared node sources (`utxo_snapshot`, `snapshot_fetch`, `chainparams`, `fetchassumeutxomanifest` RPC) updated the same way.
- GUI Fast Sync dialog uses the same preflight helper.

## Note
Rebuild Windows/Linux packages from this source before shipping installers. Apt package rebuild uses `VERSION=1.14.103`.