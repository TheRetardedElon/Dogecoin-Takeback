# MemeStream ↔ Dogecoin Core Pro — integration handoff

**Updated:** 2026-08-14  
**Sources:** GPE server env + Qt leftover (`src/qt/memestream*`) + ImGui `DrawMemeStream`

## Status matrix

| Feature | ImGui 1.14.104 (shipped) | Qt leftover (not packed) | GPE prod |
|---------|--------------------------|--------------------------|----------|
| Feed view | Home right rail = GPE site WebView. Meme Stream tab = full page + Submit (not a second native feed). | Native HTTP leftover | OK |
| POST publish | ImGui Publish tab (`X-MemeStream-Key`) | Done | Key matches built-in |
| POST publish + image | Multipart field `image` (≤ 69 KiB) | Done | Same `/publish` route |
| POST like / Wow | ImGui Wow on cards | Done | Public* |
| Tip | `sendtoaddress` from card or Tip tab | Send-coins flow | On-chain |
| **Images / media** | WinINet + stb_image (PNG/JPEG/GIF) | URL + WIC JPEG fallback | Live `/media/memestream/*` is real image bytes (2026-08-13) |

ImGui client: `pro-gui/src/memestream_http.*` + `DrawMemeStream` / `DrawMemeRail`. Home rail and Stream tab are the GPE site WebView. Submit is native HTTP. There is not a second native feed.

## Auth (publish only)

| Side | Setting |
|------|---------|
| GPE server | `MEMESTREAM_PUBLISH_KEY` |
| Core CLI | `-memestreamkey=<same>` (optional; **built-in obfuscated key already matches prod**) |
| HTTP | `X-MemeStream-Key: <same>` |

Also accepted by GPE (not preferred): `Authorization: MemeStream <key>`, body/query `memestreamKey`, headers `X-Dogecoin-Client-Key` / `X-Meme-Stream-Key`.

**Not** for MemeStream: Bearer JWT, `X-Api-Key`, GPE session cookies.

**Scope:** only POST publish + POST upload on memestream routes.

Core built-in key (decoded) matches GPE env key from 2026-08 handoff. Prefer built-in; override with `-memestreamkey=` if env rotates.

## API bases (Core uses)

| Base | Notes |
|------|--------|
| `https://gopastearth.com` | Core default (`-memestreambaseurl`) |
| Paths under `/api/public/memestream/...` | Canonical |

| Method | Path | Auth |
|--------|------|------|
| GET | `/api/public/memestream/feed?limit=&cursor=` | Public |
| POST | `/api/public/memestream/publish` | `X-MemeStream-Key` |
| POST | `/api/public/memestream/upload` | `X-MemeStream-Key` (≤ 69 KiB) |
| POST | `/api/public/memestream/items/:id/like` | Public* (+ wallet header/body) |

SPA view-only: `https://memestream.gopastearth.com`

## Images — root cause of “no media in Core”

Feed returns relative paths, e.g.:

```text
"imageUrl": "/media/memestream/-iolmTBEnbRh7ASQ.png"
```

Core resolves to:

```text
https://gopastearth.com/media/memestream/-iolmTBEnbRh7ASQ.png
```

**Live probe (2026-08-09):** that URL returns `HTTP 200` with `Content-Type: text/html` (GPE SPA shell, ~1.4 KB), **not** `image/png`.  
`QPixmap::loadFromData` fails → cards show text only (what you saw).

This is **not** a Core SSL/feed bug (feed already works). It is **GPE static/media routing**: `/media/*` is swallowed by the SPA catch-all instead of nginx/`express.static` / object storage.

### Fix on GPE (required for images in Core + web)

1. Serve `/media/memestream/*` as **static files** (or redirect to public object storage) **before** SPA fallback.  
2. Response must be real image bytes: `Content-Type: image/png|jpeg|gif|webp`.  
3. Optional improvement: feed returns **absolute** public CDN URLs so clients do not guess.  
4. CORS not required for Core Qt (no browser CORS); still needed for web SPA if media is cross-origin.

### Core client mitigations (done / partial)

- Per-request system CAs (no Windows ROOT inject crash path)  
- Auto-refresh feed on rail/page  
- Detect HTML/JSON body on image GET and show “(media not served — GPE /media route)” instead of silent blank  
- Log `MemeStream: image URL returned non-image …` to `debug.log`

## Publish body (author = tip target)

```json
{
  "title": "…",
  "body": "…",
  "wallet": "D…receive address from Core…"
}
```

Images: max **69 KiB** (70656); JPEG/PNG/GIF/WebP via multipart field `image` (etc.).

## Core does **not** need for MemeStream

- Dogecoin RPC for posting  
- POS `DOGE_RPC_*` vars (unrelated GPE path)

## Paste-back to GPE session

> Core feed works. Images fail because `GET https://gopastearth.com/media/memestream/<file>` returns SPA HTML (200 text/html), not the file. Please fix nginx/static so `/media/memestream/*` is real media before SPA fallback. Optional: absolute `imageUrl` in feed JSON. Publish key already matches Core built-in.
