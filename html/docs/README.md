# Dogecoin Dev — HTML documentation

Living project documentation for the `C:\dogedev` worktree (Dogecoin Core Pro, 1.14 DNA).

## Open the docs

Open **`index.html`** in a browser (double-click or “Open with”).

```
html/docs/index.html
```

No server required (`file://` works). Styles, diagrams, and nav are local under `assets/`.

## Structure

| Path | Purpose |
|------|---------|
| `index.html` | Dashboard / status at a glance (1.14.104) |
| `pages/how-it-works.html` | **End-user ecosystem map** (ImGui, roles, storage, cloud, testnet) |
| `pages/install-roles.html` | Client / Server / Hybrid |
| `pages/testnet.html` | Start Menu / flags / green TUI / Matrix GUI |
| `pages/` | Workstream and architecture pages |
| `pages/ibd-and-p2p.html` | IBD telemetry, stall rescue, ASMAP, parallelism |
| `pages/assumeutxo.html` | AssumeUTXO dual chainstate A–D3 + product P1 |
| `pages/fast-sync.html` | **Plain-language Fast Sync** (CDN, hashes, P2P, when to use) |
| `pages/gpenode.html` | **GPENode operator** — dump + CDN roles, mesh M1, clean kit (no secrets) |
| `pages/multi-operator-mesh.html` | Multi-operator dumpers + CDN mirrors (speed, failover, trust) |
| `pages/fast-sync-threat-model.html` | CDN threat model vs live GPE layout (no overkill) |
| `pages/storage-stack.html` | Engines + tiered storage / CDN plan |
| `pages/arcade.html` | Arcade (ImGui WebView / GPE hub; Qt cabinet still in source) |
| `pages/modern-ui.html` | Historical Qt shell inventory; product desktop is ImGui |
| `pages/memestream-integration.html` | GPE HTTP contract; Home rail + Stream/Submit |
| `pages/payment-layer.html` | Native DOGE merchants / Core Pro vs optional GPE |
| `pages/pure-doge-strategy.html` | North-star strategy (no EVM product path) |
| `pages/diagrams.html` | **Canonical diagrams** — master + Fast Sync sequence + dual chainstate (Eraser) |
| `assets/styles.css` | Shared theme + flow styles |
| `assets/nav.js` | **Shared sidebar** (auto-injected on every page) |
| `assets/diagrams/` | PNG/SVG assets (`master-diagram-latest.png`, `fastsync-diagram.png`, `assumeutxo-diagram.png`, legacy post-migration) |
| `assets/ui-reference/` | Core Pro screenshots |

Source exports for re-render live in repo root **`-FlowDiagramLatest/`** (PNG + `updatedmermaid.txt`). After Eraser export, copy PNGs into both that folder and `assets/diagrams/`.

Private operator secrets (IPs, passwords, keys) are **not** part of this public docs site.
GPENode ops scripts live in the GPENode worktree `deploy/` (see public page `gpenode.html` for the product shape).

## Design notes in `doc/` (markdown)

| File | Topic |
|------|--------|
| `doc/assumeutxo-dogecoin-1.14.md` | AssumeUTXO engineering A–D3 + product links |
| `doc/tiered-storage-and-fast-sync.md` | **Authoritative** P1–P4 tiered storage / Fast Sync plan |
| `doc/ibd-p0-peer-telemetry.md` | IBD P0 notes |

Root **`DOGECOIN_CHANGELOG.md`** carries the heavy engineering changelog (2026 node + product).

## When to update

- After a phase completes (roadmap + dashboard same day)
- After a successful or failed build/release
- When a feature is implemented or proven absent
- When architecture understanding improves
- When Eraser diagrams are re-exported (refresh `assets/diagrams/` + `diagrams.html` captions)
- New pages: add HTML under `pages/` **and** a link in `assets/nav.js`

Code wins arguments; these pages capture what we know so recovery is not only chat memory.

## Status snapshot (2026-08-14)

| Area | State |
|------|--------|
| AssumeUTXO eng A–D3 | Done |
| Fast Sync product P1 | ~90% shipped (map 6324519, CDN, WinHTTP, FastSyncDialog, 1.14.102 packages) |
| Mesh | M0–M1 live; M2 multi-URL failover next |
| Diagrams | Master + sequence + dual chainstate published under `assets/diagrams/` |
