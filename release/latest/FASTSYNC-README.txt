Dogecoin Core Pro — dogecoin-qt-fastsync.exe
============================================

WHAT IT IS
----------
Standalone Windows GUI of Dogecoin Core Pro with Fast Sync enabled
(WinHTTP HTTPS CDN download + Settings → Fast Sync from CDN…).

Same full-node client as dogecoin-qt.exe inside the zip/installer:
same Dogecoin mainnet, same consensus, same DOGE.
Not a new coin. Not a hard fork. Not a separate network.

WHAT IT IS FOR
--------------
• Optional GitHub asset for testers who want a clearly named Fast Sync GUI
• Same binary lineage as the package dogecoin-qt.exe

WHAT IT IS NOT
--------------
• Not a light client — still a full node
• Not the CDN/GPENode server (that hosts snapshots; this is the client)
• Not required if you install the setup or use the zip (use dogecoin-qt.exe there)
• Not "trust the cloud forever" — fail-closed file hash + background P2P prove

HOW TO USE
----------
1. Prefer a NEW empty datadir for first Fast Sync tests.
2. Run dogecoin-qt-fastsync.exe  (or dogecoin-qt.exe from zip/setup).
3. Settings → Fast Sync from CDN…
CDN: https://sync.doge.gopastearth.com/latest.json

RECOMMENDED FOR MOST PEOPLE
---------------------------
• dogecoin-1.14.102-win64-setup.exe
• dogecoin-1.14.102-win64.zip

Pre-release. Review before large balances.
