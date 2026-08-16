#pragma once

#include <string>

/** Folder we tell users to fill: <install>\\tor  (next to dogecoin-pro-gui.exe). */
std::string ExpectedTorDir();
std::string ExpectedTorExe();

/** Locate tor.exe. Prefers <install>\\tor\\tor.exe (optional drop-in). Cached. */
std::string FindTorExecutable();

/** True if 127.0.0.1:socksPort accepts TCP (Tor SOCKS is up). Cached, never blocks the UI. */
bool TorSocksListening(int socksPort = 9050);

/** Drop FindTor / SOCKS caches (after the user drops tor.exe or we start it). */
void InvalidateTorStatusCache();

/**
 * Start tor.exe with SocksPort 127.0.0.1:9050 only (no ControlPort).
 * Phase 1 is outbound P2P. Does not touch WebView or RPC.
 * Returns true if SOCKS is already up or comes up within a few seconds.
 */
bool StartLocalTor(const std::string& torExe, const std::string& dataDir, std::string& errOut);
