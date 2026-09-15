# Dogecoin Core Pro 1.14.106

GUI-only. `dogecoind` is the same 1.14.105 daemon.

Open source: https://github.com/TheRetardedElon/Dogecoin-Takeback

## Windows

| File | What |
|------|------|
| `dogecoin-1.14.106-win64-setup-rpcsecure.exe` | NSIS setup (recommended) |
| `dogecoin-1.14.106-win64.zip` | Portable tree |
| `SHA256SUMS-1.14.106.txt` | Digests |

## This release

- Will not start or attach if official Dogecoin Core (`dogecoin-qt`) or another `dogecoind` is already running
- File → Exit / tray **Quit and stop node**: RPC `stop`, wait for flush, then stop `DogecoinGPENode` (Windows) or `dogecoin-core-pro` (Linux systemd) so the service cannot bring the node back
- X / hide still goes to the tray; the node stays up
- Linux source has the same GUI rules; this tag’s packaged `.deb` is still 1.14.105 (daemon unchanged)

Do not force-kill during the shutdown splash.
