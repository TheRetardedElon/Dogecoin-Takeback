# Privacy Policy

**Product:** Dogecoin Core Pro (Dogecoin-Takeback)  
**Effective:** 2026-08-11  
**Canonical URL:** https://github.com/TheRetardedElon/Dogecoin-Takeback/blob/main/PRIVACY.md  

Related operator kit: [Dogecoin GPENode Privacy Policy](https://github.com/TheRetardedElon/Dogecoin-GPENode/blob/main/PRIVACY.md)

---

## Summary

Dogecoin Core Pro is a **local** Dogecoin full-node wallet and desktop application. It processes blockchain and wallet data **on your device**. We do **not** provide a mandatory cloud account for using the wallet, and we do **not** sell your personal information as a product of this software.

Running a node and using optional online features will cause **network traffic** you initiate or that is required for consensus (P2P). That is different from a proprietary analytics SDK phoning home with a user profile.

---

## Who we are

“We” means the maintainers of the open-source project [TheRetardedElon/Dogecoin-Takeback](https://github.com/TheRetardedElon/Dogecoin-Takeback) (Dogecoin Core Pro builds and docs).

Contact for privacy questions: open a GitHub issue on that repository, or use the contact method on the maintainer’s GitHub profile.

---

## Data on your device

Typical local data includes:

| Data | Examples | Notes |
|------|----------|--------|
| Wallet | `wallet.dat`, keys, labels | **Never share**; we do not receive these through the app by default |
| Chain data | blocks, chainstate, indexes | Large; stored under your datadir |
| Config | `dogecoin.conf`, RPC credentials | Unique passwords may be written by installers |
| UI preferences | Qt settings | Local only |
| Logs | `debug.log` | May include IPs of peers and RPC errors |

You control the datadir location (including installer-generated paths under AppData / ProgramData style locations on Windows).

---

## Network activity

### Required for a normal online node

- **Dogecoin P2P:** connections to peers, block/transaction relay, addresses  
- **DNS seeds / peers:** discovery of network participants  

Peer operators are third parties we do not control.

### Optional or feature-dependent

Depending on build and settings, the application may:

| Feature | Possible external contact | Data involved |
|---------|---------------------------|---------------|
| **Fast Sync / AssumeUTXO** | HTTPS CDN you use (e.g. static snapshot host) | Download of snapshot metadata and multi-GB files; your IP to that host |
| **Meme Stream / media** | Remote media or API hosts configured for the feature | URLs, content you choose to load; see in-app sources |
| **Updates / documentation links** | Sites you open | Normal browser/HTTPS behavior |
| **RPC** | Only addresses **you** configure (prefer localhost) | Wallet/node control — treat as sensitive |

Default packaging aims to keep **RPC on localhost** unless you change it.

### What we do not do by default

- No requirement to create an account with us to send/receive DOGE  
- No sale of wallet contents or seed phrases  
- No advertising network embedded as a condition of using Core Pro  

If optional diagnostics or online services are added later, they should be documented and prefer opt-in.

---

## Installers and downloads

When you download installers or packages from:

- GitHub Releases  
- Our or community static hosts / apt mirrors  
- Third-party mirrors you choose  

those hosts may log standard web request metadata (IP, file path, time). That is distribution infrastructure, not wallet telemetry.

Windows SmartScreen / antivirus products may scan installers under **their** policies.

---

## Blockchain transparency

Dogecoin is a public ledger. Transactions you broadcast are **public** by design (amounts, addresses, timing). That is a property of the network, not a private database we operate.

---

## Children

This software is intended for adults and operators capable of securing cryptocurrency keys. It is not directed at children under 13 (or the minimum age in your jurisdiction).

---

## Your choices

- Use offline / air-gapped workflows only if you understand the limits (no sync, no broadcast).  
- Disable or avoid optional CDN / media features if you do not want those HTTPS requests.  
- Uninstall and securely delete the datadir (and backups) to remove local wallet and chain data.  
- Keep RPC credentials private; do not expose port 22555 to the internet.

See [SECURITY.md](./SECURITY.md) if present in this repository.

---

## Changes

Updates to this policy are published by changing this file in the repository. The **Effective** date will be revised when material changes occur.

---

## License vs privacy

Source distribution is under the project [LICENSE](./COPYING) / applicable license files. Licensing is separate from this privacy notice.
