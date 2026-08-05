// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_NODE_UTXO_SNAPSHOT_H
#define DOGECOIN_NODE_UTXO_SNAPSHOT_H

#include "fs.h"
#include "protocol.h"
#include "serialize.h"
#include "tinyformat.h"
#include "uint256.h"

#include <cstring>
#include <ios>
#include <stdint.h>
#include <string>

class CAutoFile;
class CChainState;
class CCoinsView;

/**
 * Dogecoin 1.14 AssumeUTXO snapshot format (Phase B).
 *
 * Wire layout (SER_DISK):
 *   magic[5]           "utxo\xff"
 *   version            uint16 (1)
 *   network_magic[4]   chain MessageStart
 *   base_blockhash     uint256
 *   coins_count        uint64  (number of CCoins / txid records)
 *   repeated coins_count times:
 *     txid             uint256
 *     coins            CCoins (existing per-tx serialization)
 *
 * Note: Bitcoin Core's modern snapshot uses per-outpoint Coin records.
 * Dogecoin 1.14 still uses per-txid CCoins; version 1 marks this layout.
 */
static const unsigned char SNAPSHOT_MAGIC_BYTES[5] = {'u', 't', 'x', 'o', 0xff};
static const uint16_t SNAPSHOT_VERSION = 1;

/** Metadata header for a UTXO snapshot file. */
class SnapshotMetadata
{
public:
    CMessageHeader::MessageStartChars network_magic;
    uint256 base_blockhash;
    uint64_t coins_count;

    SnapshotMetadata();
    explicit SnapshotMetadata(const CMessageHeader::MessageStartChars& networkMagicIn);
    SnapshotMetadata(const CMessageHeader::MessageStartChars& networkMagicIn,
                     const uint256& baseBlockhashIn,
                     uint64_t coinsCountIn);

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        for (int i = 0; i < 5; i++) {
            ::Serialize(s, SNAPSHOT_MAGIC_BYTES[i]);
        }
        uint16_t version = SNAPSHOT_VERSION;
        ::Serialize(s, version);
        s.write(reinterpret_cast<const char*>(network_magic), CMessageHeader::MESSAGE_START_SIZE);
        ::Serialize(s, base_blockhash);
        ::Serialize(s, coins_count);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        unsigned char magic[5];
        for (int i = 0; i < 5; i++) {
            ::Unserialize(s, magic[i]);
        }
        if (memcmp(magic, SNAPSHOT_MAGIC_BYTES, 5) != 0) {
            throw std::ios_base::failure(
                "Invalid UTXO snapshot magic bytes (not a Dogecoin 1.14 snapshot, or corrupt file)");
        }
        uint16_t version = 0;
        ::Unserialize(s, version);
        if (version != SNAPSHOT_VERSION) {
            throw std::ios_base::failure(
                strprintf("Unsupported UTXO snapshot version %u (need %u)", version, SNAPSHOT_VERSION));
        }
        s.read(reinterpret_cast<char*>(network_magic), CMessageHeader::MESSAGE_START_SIZE);
        ::Unserialize(s, base_blockhash);
        ::Unserialize(s, coins_count);
    }
};

/**
 * Write the UTXO set of `view` (typically after FlushStateToDisk on the active tip)
 * to `path`. Overwrites an existing file only after writing a temp file successfully.
 *
 * @param view        coins view to dump (must be flushed; Cursor() iterates LevelDB)
 * @param base_hash   tip block hash represented by the UTXO set
 * @param base_height tip height (reported only)
 * @param path        destination path
 * @param coins_written out: number of CCoins records written
 * @param error       out: human-readable error
 * @return true on success
 */
bool WriteUTXOSnapshot(CCoinsView* view,
                       const uint256& base_hash,
                       int base_height,
                       const fs::path& path,
                       uint64_t& coins_written,
                       std::string& error);

/**
 * Load a snapshot file into the idle background chainstate:
 *  - allocates chainstate_snapshot/ LevelDB (wiped)
 *  - deserializes all CCoins
 *  - sets background tip if base hash is in mapBlockIndex
 *
 * Does not swap the active (wallet/net) chainstate. Call ActivateLoadedSnapshot()
 * (RPC activatesnapshot / loadtxoutset activate=true) for Phase B2 tip promotion.
 *
 * Requires InitializeBackgroundChainstate().
 */
bool LoadUTXOSnapshot(const fs::path& path,
                      uint64_t& coins_loaded,
                      uint256& base_hash,
                      int& base_height,
                      std::string& error);

/** Directory name under datadir for the background snapshot coins DB. */
static const char* const SNAPSHOT_CHAINSTATE_DIR = "chainstate_snapshot";

#endif // DOGECOIN_NODE_UTXO_SNAPSHOT_H
