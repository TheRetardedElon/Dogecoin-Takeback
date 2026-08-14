Optional Tor for Dogecoin Core Pro
=================================

Core Pro does NOT install Tor. That keeps Windows Defender / AV from
flagging the setup. Privacy is opt-in.

If you want blockchain P2P (peers, blocks, transactions) hidden from
your ISP:

  1. Download the Windows "Tor Expert Bundle" from https://www.torproject.org/
  2. Unzip so this file exists:

       <this folder>\tor.exe

     Example:

       C:\Program Files\Dogecoin\tor\tor.exe

  3. Open Core Pro → Options → Privacy
  4. Check "Route blockchain P2P through local Tor" (optional; off by default)
     Core Pro starts tor.exe only when this is on, including the next launch.
  5. After IBD, File → Exit, then reopen so dogecoind picks up proxy=

Meme Stream and Arcade stay on normal HTTPS. They are not routed
through Tor. Local RPC (127.0.0.1) is never sent through Tor.

Leave the checkbox off to keep ordinary clearnet P2P.
