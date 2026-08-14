Dogecoin Core 0.14.2
=====================

Development
---------------------
The Dogecoin repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Travis CI](travis-ci.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
* Discuss on the [BitcoinTalk](https://bitcointalk.org/) forums, in the [Dogecoin thread](https://bitcointalk.org/index.php?topic=361813.0).
* Discuss on [#dogecoin-dev](http://webchat.freenode.net/?channels=dogecoin-dev) on Freenode. If you don't have an IRC client use [webchat here](http://webchat.freenode.net/?channels=dogecoin-dev).

### Core Pro (product)
- [Install roles — Client / Server / Hybrid](install-roles.md)
- [HTML docs (open offline)](../html/docs/index.html) — how it works, testnet, diagrams, storage
- [Privacy / optional Tor](privacy-transport.md) — P2P only, not bundled
- [Tiered storage & Fast Sync](tiered-storage-and-fast-sync.md)
- [Client startup & splash breakdown](client-startup-splash-breakdown.md)
- [Startup performance](startup-performance.md) — LevelDB default, MDBX opt-in
- [MDBX from a wiped datadir](mdbx-fresh-start.md)
- [Qt phase-out](qt-phase-out.md) — 1.14.104 ships ImGui; Qt not in installer
- [ImGui control plane (`pro-gui`)](pro-gui-imgui.md)
- [Meme Stream / GPE handoff](memestream-gpe-handoff.md)
- [AssumeUTXO on 1.14 DNA](assumeutxo-dogecoin-1.14.md)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
This product includes software developed by the Bitcoin developers for use in [Dogecoin Core](https://www.bitcoin.org/). 
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
