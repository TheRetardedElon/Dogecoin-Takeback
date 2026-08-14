# GPE Arcade unlock contract (Core Pro)

Verified on **https://arcade.gopastearth.com/** (not `gopastearth.com/arcade`).

GPE unlocks play based on **host identity**, not Qt vs Dear ImGui.

## Live unlock behavior

| Request | Result |
|---------|--------|
| Normal browser | **LOCKED** — “Unlock play in Dogecoin Core Pro” |
| Header `X-Dogecoin-Core-Pro: 1` | Play unlocked · source `core-pro` |
| UA contains `DogecoinCorePro/1.14.103` | Play unlocked |
| Legacy `X-Arcade-Key` | Still works |

## Core Pro must send (any one is enough)

1. **Header (best for WebView2 embed):** `X-Dogecoin-Core-Pro: 1`  
   Optional: `X-Dogecoin-Core-Pro-Build: 1.14.103`
2. **User-Agent token:** include `DogecoinCorePro/<version>`  
   e.g. `… Safari/537.36 DogecoinCorePro/1.14.103`
3. **JS inject (helper, not sole gate):**
   ```js
   window.__DOGECOIN_CORE_PRO__ = true;
   window.__DOGECOIN_CORE_PRO_BUILD__ = '1.14.103';
   ```
4. Do **not** rely on `?corepro=1` alone (bookmarkable).

## Session

SPA: `POST /api/public/arcade/session` → httpOnly cookie `gpe_arcade_play` (6h).

## UX

- Locked → **LOBBY**
- Unlocked → **IN CORE** + play enabled

## Current pro-gui implementation

- Primary URL: `https://arcade.gopastearth.com/`
- `ArcadeHost::LaunchCabinetUnlocked()` opens Edge/Chrome **app window** with Core Pro UA token
- Full in-client WebView2 (headers + JS inject + cabinet chrome) is the next packaging step

ImGui and Qt must send the **same** identity strings.
