# Network privacy for Core Pro — discussion (not shipping yet)

**Status:** Phase 1 shipping in ImGui **Options → Privacy**. P2P via local Tor SOCKS only.  
**Date:** 2026-08-14

**Locked answers**

1. Daily wallet = outbound Tor only. Inbound `.onion` stays on an Advanced / Operator network tab.
2. IBD and Fast Sync stay clearnet. Tor-only IBD is a buried, warned option.
3. **Tor is only for chain / mining wire traffic** (`dogecoind` P2P: peers, blocks, txs, AuxPoW). Meme Stream, Arcade, and any other WebView/HTTPS stay on a normal web connection. Do not tunnel WebView through SOCKS. Do not claim “the whole app is on Tor.”
4. Do **not** bundle Tor in the installer. Users drop the Expert Bundle at `<install>\\tor\\tor.exe` (e.g. `C:\\Program Files\\Dogecoin\\tor\\tor.exe`). Optional; default is clearnet P2P.
5. GUI ↔ node RPC stays `127.0.0.1`. `getblocktemplate` / `submitblock` never go through Tor.

Gemini’s split is the right one. “ISP cannot snoop” and “nobody knows my IP” are **different products**. Dogecoin Core Pro 1.14 already has hooks for the second. It does **not** have the first.

## What we actually have in-tree (1.14)

| Capability | In this tree? | How |
|---|---|---|
| Plaintext P2P (magic bytes on the wire) | Yes — default | Same as Bitcoin ≤25 / Dogecoin 1.14 |
| BIP 324 v2 encrypted transport | **No** | Zero hits for `v2transport` / `bip324` |
| SOCKS5 `-proxy=127.0.0.1:9050` | Yes | Routes outbound P2P (and optionally RPC if misconfigured — do not) |
| `-listenonion` / Tor control | Yes | `DEFAULT_LISTEN_ONION = true` — creates a `.onion` **if** Tor’s control port is up |
| `-onlynet=onion` | Yes | Outbound only to onion peers |
| Bundled Tor daemon | **No** | User must install Tor (or we would vendor it later) |

ImGui already has proxy fields on **Options → Network**. They are conf-intent, not a “Darknet Mode” product.

## Two layers (do not merge them in the UI)

### A. Transport encryption (BIP 324)

**Goal:** ISP / coffee-shop Wi‑Fi sees random bytes, not Dogecoin magic. Peers still see your IP.

- Bitcoin Core shipped this recently. Dogecoin 1.14 has **not** ported it.
- Porting is a **P2P protocol** change (version bits / handshake). That is **not** Path A GUI work. It belongs in `dogecoind` after a real review against Bitcoin’s BIP 324, with a long dual-stack period (v1 + v2).
- “Strict: only encrypted peers” on a network that has **no** v2 peers yet = **you cannot sync**. Do not ship a force-toggle until Dogecoin mainnet has enough v2 nodes.
- Latency: negligible. No extra process.

**If we ever do it:** dogecoind first, then an ImGui status (“session encrypted / plaintext”) and only later an optional “prefer v2” switch. Never “force v2” as the first control.

### B. Tor / onion (already almost product-ready)

**Goal:** ISP sees “this machine talks to Tor.” Peers see an onion, not your home IP.

What we can do **without** changing consensus:

1. Detect Tor on `127.0.0.1:9050` (SOCKS) and `9051` (control).
2. Write `proxy=127.0.0.1:9050`, keep `listenonion=1` (already default).
3. Optional “outbound via Tor only”: `onlynet=onion` — **do not enable during IBD**.
4. Show the `.onion` address in Network once `getnetworkinfo` / debug.log has it.

**Do not** route localhost RPC through Tor. GUI ↔ `dogecoind` stays `127.0.0.1`.

**Do not** auto-spawn a bundled Tor in v1. Shipping a hidden Tor process is a support, legal, and update-surface problem (exit policy, bridges, antivirus). First product: “Use the Tor you already installed.” Bundle later if operators ask.

**IBD:** Tor-only IBD is slow and fragile. Product rule: Fast Sync / IBD on clearnet (or BIP 324 once it exists), then optional Tor for **steady-state** wallet use.

## What Gemini got wrong for *this* repo

- “Bitcoin Core recently rolled out BIP 324” is true for **Bitcoin**. It is **not** in Dogecoin 1.14.103 / this tree. Promising a “Strict Transport Encryption” toggle today would be a lie.
- “Darknet Mode automatically bundles a lightweight Tor” is a later phase, not a weekend switch.
- RPC through Tor is not a privacy win for a local GUI. It is a foot-gun.

## Recommended product sequence

| Phase | Ship | Does not ship |
|---|---|---|
| **0 — now** | Honest Network copy: “P2P is plaintext unless you set a Tor proxy.” Options already have SOCKS fields. | BIP 324, bundled Tor |
| **1** | Options → Privacy: optional drop-in `<install>\\tor\\tor.exe`, Start Tor, P2P checkbox (off by default). No installer bundle. | WebView-via-Tor, forced Tor, bundled tor.exe |
| **2** | After a Dogecoin BIP 324 port exists and has mainnet peers: “Prefer encrypted sessions” + peer padlock | Force-v2 on a v1-only network |
| **3 (maybe)** | Optional vendored Tor Expert Bundle, signed, off by default | Transparent “always on VPN” claims |

Web / Arcade / Meme Stream are **decided: not Tor**. Remaining product choice is only when to turn P2P Tor on (after IBD vs always, operator onion inbound).

## Threat model (what the split actually costs)

Malware that already runs as the same Windows user as the wallet **wins anyway** (it can read `wallet.dat`). New surfaces are what a *lesser* local process or a confused user can do.

| Plane | Real on this tree? | Already covered | Still open |
|---|---|---|---|
| Localhost RPC (`127.0.0.1:22555`) | Yes — Path A *requires* it | Bind 127.0.0.1. Unique installer password in `dogecoin.conf`. GUI prefers `.cookie` then conf; **does not write the password into `pro-gui-settings.ini`**. | Hybrid still uses conf password (LocalSystem cookie ACL). Unencrypted wallet + those creds = spend. Encrypt nudge is in Options → Wallet. |
| Proxy “fail-open” to clearnet | **No** | With `-proxy=` set, Core only connects via SOCKS. Dead Tor ⇒ connect fails, it does **not** dial peers directly. DNS names go through the same proxy (`SetNameProxy`). | User thinks they are on Tor while IBD is still clearnet (we wait for a clean restart). Two UIs (Privacy vs Network) can disagree. |
| User-supplied `Dogecoin\tor\tor.exe` | Yes — we exec it when they opt in | Not bundled, not auto-started | A trojan named `tor.exe` in that folder runs as the user. Tell them to take the Expert Bundle from torproject.org only. |
| Archive / Fast Sync | Only if they use those features | Fast Sync has SHA-256. Consensus still rejects a block that does not hash. | Compromised local `chainstate` is “machine already owned.” |
| MDBX mmap | Overstated | Same user isolation as LevelDB. PE is a normal Windows ASLR/DEP binary. | Unclean kill can lose unflushed tip (we already hit this). Not a remote exploit. |
| WebView2 (Meme / Arcade) | Separate process, HTTPS | Not on Tor; we said so. Tips go through our RPC, not the page. | Treat GPE like a website. Do not expose RPC to the WebView. |

**Do not promise:** “RPC cookie fixes malware.” Same-user malware reads `.cookie` too. Cookie is better against *stale* stolen conf passwords and against copying the datadir while the node is stopped.

Path A stays: GUI writes `dogecoin.conf` / RPC; `dogecoind` owns the wire.
