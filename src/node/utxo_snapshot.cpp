// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node/utxo_snapshot.h"

#include "chainparams.h"
#include "coins.h"
#include "hash.h"
#include "node/chainstate.h"
#include "streams.h"
#include "txdb.h"
#include "util.h"
#include "validation.h"
#include "version.h"

#include <boost/thread.hpp>

#include <cstring>
#include <memory>

SnapshotMetadata::SnapshotMetadata() : coins_count(0)
{
    memset(network_magic, 0, sizeof(network_magic));
}

SnapshotMetadata::SnapshotMetadata(const CMessageHeader::MessageStartChars& networkMagicIn)
    : coins_count(0)
{
    memcpy(network_magic, networkMagicIn, CMessageHeader::MESSAGE_START_SIZE);
}

SnapshotMetadata::SnapshotMetadata(const CMessageHeader::MessageStartChars& networkMagicIn,
                                   const uint256& baseBlockhashIn,
                                   uint64_t coinsCountIn)
    : base_blockhash(baseBlockhashIn), coins_count(coinsCountIn)
{
    memcpy(network_magic, networkMagicIn, CMessageHeader::MESSAGE_START_SIZE);
}

bool ComputeCoinsHashSerialized(CCoinsView* view, uint256& hash_out, std::string& error)
{
    hash_out.SetNull();
    error.clear();
    if (!view) {
        error = "No coins view";
        return false;
    }
    std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());
    if (!pcursor) {
        error = "Unable to open coins cursor for hash";
        return false;
    }

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    const uint256 hashBlock = pcursor->GetBestBlock();
    ss << hashBlock;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        uint256 key;
        CCoins coins;
        if (!pcursor->GetKey(key) || !pcursor->GetValue(coins)) {
            error = "Unable to read coins while hashing UTXO set";
            return false;
        }
        ss << key;
        for (unsigned int i = 0; i < coins.vout.size(); i++) {
            const CTxOut& out = coins.vout[i];
            if (!out.IsNull()) {
                ss << VARINT(i + 1);
                ss << out;
            }
        }
        ss << VARINT(0);
        pcursor->Next();
    }
    hash_out = ss.GetHash();
    return true;
}

static bool CountCoins(CCoinsView* view, uint64_t& count, std::string& error)
{
    count = 0;
    std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());
    if (!pcursor) {
        error = "Unable to open coins cursor";
        return false;
    }
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        uint256 key;
        CCoins coins;
        if (!pcursor->GetKey(key) || !pcursor->GetValue(coins)) {
            error = "Unable to read coins while counting";
            return false;
        }
        if (!coins.IsPruned()) {
            ++count;
        }
        pcursor->Next();
    }
    return true;
}

bool WriteUTXOSnapshot(CCoinsView* view,
                       const uint256& base_hash,
                       int base_height,
                       const fs::path& path,
                       uint64_t& coins_written,
                       std::string& error)
{
    coins_written = 0;
    error.clear();

    if (!view) {
        error = "No coins view";
        return false;
    }
    if (base_hash.IsNull()) {
        error = "Base block hash is null";
        return false;
    }

    uint64_t expected = 0;
    if (!CountCoins(view, expected, error)) {
        return false;
    }

    fs::path path_tmp = path;
    path_tmp += ".tmp";

    FILE* file = fsbridge::fopen(path_tmp, "wb");
    if (!file) {
        error = strprintf("Could not open %s for writing", path_tmp.string());
        return false;
    }
    CAutoFile afile(file, SER_DISK, CLIENT_VERSION);

    SnapshotMetadata metadata(Params().MessageStart(), base_hash, expected);
    try {
        afile << metadata;
    } catch (const std::exception& e) {
        error = strprintf("Failed to write snapshot metadata: %s", e.what());
        return false;
    }

    std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());
    if (!pcursor) {
        error = "Unable to open coins cursor for dump";
        return false;
    }

    uint64_t written = 0;
    try {
        while (pcursor->Valid()) {
            boost::this_thread::interruption_point();
            uint256 txid;
            CCoins coins;
            if (!pcursor->GetKey(txid) || !pcursor->GetValue(coins)) {
                error = "Unable to read coins while writing snapshot";
                return false;
            }
            if (!coins.IsPruned()) {
                afile << txid;
                afile << coins;
                ++written;
            }
            pcursor->Next();
        }
    } catch (const std::exception& e) {
        error = strprintf("Failed while writing coins: %s", e.what());
        return false;
    }

    if (written != expected) {
        error = strprintf("Coin count mismatch while writing (wrote %llu, expected %llu)",
                          (unsigned long long)written, (unsigned long long)expected);
        return false;
    }

    // CAutoFile fclose via release + fclose
    FILE* raw = afile.release();
    if (raw && fclose(raw) != 0) {
        error = strprintf("Error closing temporary snapshot file %s", path_tmp.string());
        return false;
    }

    if (fs::exists(path)) {
        fs::remove(path);
    }
    try {
        fs::rename(path_tmp, path);
    } catch (const fs::filesystem_error& e) {
        error = strprintf("Failed to rename snapshot into place: %s", e.what());
        return false;
    }

    coins_written = written;
    LogPrintf("UTXO snapshot: wrote %llu coins at height %d hash=%s to %s\n",
              (unsigned long long)written, base_height, base_hash.ToString(), path.string());
    return true;
}

bool LoadUTXOSnapshot(const fs::path& path,
                      uint64_t& coins_loaded,
                      uint256& base_hash,
                      int& base_height,
                      std::string& error)
{
    coins_loaded = 0;
    base_hash.SetNull();
    base_height = -1;
    error.clear();

    if (!HasBackgroundChainstate() || !BackgroundChainstate()) {
        error = "Background chainstate not initialized";
        return false;
    }

    CChainState& bg = *BackgroundChainstate();
    if (!bg.IsIdle() && bg.HasSnapshot()) {
        error = "Background chainstate already holds a loaded snapshot (restart to load another)";
        return false;
    }
    if (!bg.IsIdle()) {
        error = "Background chainstate is not idle";
        return false;
    }

    FILE* file = fsbridge::fopen(path, "rb");
    if (!file) {
        error = strprintf("Could not open snapshot file %s", path.string());
        return false;
    }
    CAutoFile afile(file, SER_DISK, CLIENT_VERSION);

    SnapshotMetadata metadata(Params().MessageStart());
    try {
        afile >> metadata;
    } catch (const std::exception& e) {
        error = strprintf("Unable to parse snapshot metadata: %s", e.what());
        return false;
    }

    if (memcmp(metadata.network_magic, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE) != 0) {
        error = "Snapshot network magic does not match this node's chain";
        return false;
    }
    if (metadata.base_blockhash.IsNull()) {
        error = "Snapshot base block hash is null";
        return false;
    }
    if (metadata.coins_count > 100000000ULL) {
        // Sanity cap (~100M txs) to catch corrupt headers early on 1.14 hardware.
        error = strprintf("Snapshot coins_count %llu looks unreasonable",
                          (unsigned long long)metadata.coins_count);
        return false;
    }

    base_hash = metadata.base_blockhash;

    // Allocate separate LevelDB for snapshot coins (never the live chainstate/).
    if (!bg.AllocateCoinsDB(GetDataDir() / SNAPSHOT_CHAINSTATE_DIR, /*fMemory=*/false, /*fWipe=*/true)) {
        error = "Failed to allocate background coins database (chainstate_snapshot)";
        return false;
    }

    CCoinsViewCache* cache = bg.CoinsTip();
    if (!cache) {
        error = "Background coins tip cache missing after allocate";
        bg.ResetCoinsDB();
        return false;
    }

    const size_t flush_batch = 5000;
    uint64_t loaded = 0;
    try {
        for (uint64_t i = 0; i < metadata.coins_count; i++) {
            if (i % 1000 == 0) {
                boost::this_thread::interruption_point();
            }
            uint256 txid;
            CCoins coins;
            afile >> txid;
            afile >> coins;
            if (coins.IsPruned()) {
                error = strprintf("Snapshot contains pruned coins for tx %s", txid.ToString());
                bg.ResetCoinsDB();
                return false;
            }
            {
                CCoinsModifier m = cache->ModifyCoins(txid);
                *m = coins;
            }
            ++loaded;
            if (loaded % flush_batch == 0) {
                if (!cache->Flush()) {
                    error = "Failed to flush coins during snapshot load";
                    bg.ResetCoinsDB();
                    return false;
                }
            }
        }
    } catch (const std::exception& e) {
        error = strprintf("Failed while reading snapshot coins (loaded %llu of %llu): %s",
                          (unsigned long long)loaded,
                          (unsigned long long)metadata.coins_count,
                          e.what());
        bg.ResetCoinsDB();
        return false;
    }

    cache->SetBestBlock(metadata.base_blockhash);
    if (!cache->Flush()) {
        error = "Failed to flush final snapshot coins batch";
        bg.ResetCoinsDB();
        return false;
    }

    // Attach tip if this block is already known (headers/blocks present).
    // Full activation (wallet uses snapshot tip) is a later slice.
    {
        LOCK(cs_main);
        BlockMap::iterator it = mapBlockIndex.find(metadata.base_blockhash);
        if (it != mapBlockIndex.end() && it->second) {
            bg.GetChain().SetTip(it->second);
            base_height = it->second->nHeight;
        } else {
            base_height = -1;
            LogPrintf("UTXO snapshot: base block %s not in block index yet; coins loaded, tip unset\n",
                      metadata.base_blockhash.ToString());
        }
    }

    bg.SetSnapshotInfo(metadata.base_blockhash, loaded);

    // Phase C: freeze expected UTXO hash of the assumed set at load time
    // (before activation mutates active coins above H).
    {
        uint256 coins_hash;
        std::string hash_err;
        if (!cache->Flush()) {
            error = "Failed to flush before hashing snapshot coins";
            bg.ResetCoinsDB();
            return false;
        }
        if (!ComputeCoinsHashSerialized(cache, coins_hash, hash_err)) {
            error = strprintf("Failed to hash loaded snapshot coins: %s", hash_err);
            bg.ResetCoinsDB();
            return false;
        }
        bg.SetSnapshotCoinsHash(coins_hash);
        LogPrintf("UTXO snapshot: coins hash_serialized=%s\n", coins_hash.ToString());
    }

    coins_loaded = loaded;
    LogPrintf("UTXO snapshot: loaded %llu coins base=%s height=%d from %s into background chainstate\n",
              (unsigned long long)loaded,
              metadata.base_blockhash.ToString(),
              base_height,
              path.string());
    return true;
}

bool AssumeUtxoDiskStateExists()
{
    return fs::exists(GetDataDir() / ASSUMEUTXO_STATE_FILENAME);
}

void RemoveAssumeUtxoDiskState()
{
    try {
        fs::remove(GetDataDir() / ASSUMEUTXO_STATE_FILENAME);
    } catch (...) {
    }
}

bool WriteAssumeUtxoDiskState(const AssumeUtxoDiskState& st, std::string& error)
{
    error.clear();
    fs::path path = GetDataDir() / ASSUMEUTXO_STATE_FILENAME;
    fs::path path_tmp = path;
    path_tmp += ".tmp";

    FILE* file = fsbridge::fopen(path_tmp, "wb");
    if (!file) {
        error = strprintf("Could not open %s for writing", path_tmp.string());
        return false;
    }
    CAutoFile afile(file, SER_DISK, CLIENT_VERSION);
    try {
        uint32_t version = 1;
        afile << version;
        afile << st.base_blockhash;
        afile << st.coins_hash;
        afile << st.coins_count;
        afile << st.status;
        afile << st.bg_height;
    } catch (const std::exception& e) {
        error = strprintf("Failed writing assumeutxo.dat: %s", e.what());
        return false;
    }
    FILE* raw = afile.release();
    if (raw && fclose(raw) != 0) {
        error = "Error closing assumeutxo.dat.tmp";
        return false;
    }
    if (fs::exists(path)) {
        fs::remove(path);
    }
    try {
        fs::rename(path_tmp, path);
    } catch (const fs::filesystem_error& e) {
        error = strprintf("Failed to rename assumeutxo.dat into place: %s", e.what());
        return false;
    }
    return true;
}

bool ReadAssumeUtxoDiskState(AssumeUtxoDiskState& st, std::string& error)
{
    error.clear();
    fs::path path = GetDataDir() / ASSUMEUTXO_STATE_FILENAME;
    FILE* file = fsbridge::fopen(path, "rb");
    if (!file) {
        error = strprintf("Could not open %s", path.string());
        return false;
    }
    CAutoFile afile(file, SER_DISK, CLIENT_VERSION);
    try {
        uint32_t version = 0;
        afile >> version;
        if (version != 1) {
            error = strprintf("Unsupported assumeutxo.dat version %u", version);
            return false;
        }
        afile >> st.base_blockhash;
        afile >> st.coins_hash;
        afile >> st.coins_count;
        afile >> st.status;
        afile >> st.bg_height;
    } catch (const std::exception& e) {
        error = strprintf("Failed reading assumeutxo.dat: %s", e.what());
        return false;
    }
    return true;
}
