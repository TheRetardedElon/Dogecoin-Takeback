// Copyright (c) 2019-2022 The Bitcoin Core developers
// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_ASMAP_H
#define DOGECOIN_ASMAP_H

#include <cstdint>
#include <string>
#include <vector>

class CNetAddr;

/**
 * Autonomous System (AS) map for peer diversity bucketing.
 *
 * When loaded via -asmap, CNetAddr::GetGroup() returns ASN-based groups instead
 * of IP /16 prefixes so outbound connections and addrman buckets diversify by
 * ISP/network operator rather than coincidental address layout.
 *
 * Map format is Bitcoin Core compatible (same encoder as bitcoin-core/asmap-data).
 * Off by default; enable with -asmap or -asmap=/path/to/ip_asn.map
 */

/** Interpret asmap bitstream for a 128-bit IP (IPv4-mapped or IPv6). */
uint32_t InterpretASMap(const std::vector<bool>& asmap, const std::vector<bool>& ip);

/** Sanity-check an asmap for lookups of the given input bit length (128 for IP). */
bool SanityCheckASMap(const std::vector<bool>& asmap, int bits);

/** Load and sanity-check asmap from a binary file. Empty vector on failure. */
std::vector<bool> DecodeAsmap(const std::string& path);

/** Install global asmap used by GetMappedAS / GetGroup. */
void SetAsmap(std::vector<bool> asmap);

/** True if a non-empty asmap is active. */
bool IsAsmapEnabled();

/** Number of bits in the loaded asmap (0 if disabled). */
size_t GetAsmapSize();

/**
 * Look up ASN for address using the global asmap.
 * Returns 0 if asmap disabled, address not mappable (e.g. Tor), or not found.
 * AS0 is reserved (RFC7607), so 0 is a safe "not found" sentinel.
 */
uint32_t GetMappedAS(const CNetAddr& address);

#endif // DOGECOIN_ASMAP_H
