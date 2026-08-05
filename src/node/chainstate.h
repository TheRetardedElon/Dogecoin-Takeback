// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_NODE_CHAINSTATE_H
#define DOGECOIN_NODE_CHAINSTATE_H

#include "chain.h"
#include "fs.h"
#include "uint256.h"

#include <memory>
#include <string>

class CBlockIndex;
class CChainParams;
class CCoinsViewCache;
class CCoinsViewDB;

/**
 * A chainstate is a tip (CChain) plus optional UTXO cache for that tip.
 *
 * AssumeUTXO progression:
 *  - A1/A2: active "ibd" wraps globals chainActive / pcoinsTip
 *  - A3: idle "background" owns an empty CChain
 *  - B1: background loads a UTXO snapshot into chainstate_snapshot/
 *  - B2: ActivateLoadedSnapshot() promotes snapshot to active tip
 *  - C1: StepBackgroundValidation() connects genesis→H on parked IBD coins
 *        and proves hash_serialized matches the assumed snapshot
 */
class CChainState
{
public:
    CChainState(const std::string& nameIn, CChain& chainIn, CCoinsViewCache*& coinsTipIn);
    explicit CChainState(const std::string& nameIn);
    ~CChainState();

    const std::string& GetName() const { return name; }
    void SetName(const std::string& nameIn) { name = nameIn; }

    CChain& GetChain() { return *chain_ptr; }
    const CChain& GetChain() const { return *chain_ptr; }

    CCoinsViewCache* CoinsTip() const;
    CCoinsViewCache*& CoinsTipRef();

    CBlockIndex* Tip() const;
    int Height() const;

    bool IsActiveRole() const { return active_role; }
    void SetActiveRole(bool active) { active_role = active; }

    bool IsIdle() const { return CoinsTip() == nullptr; }
    bool OwnsChain() const { return owned_chain.get() != nullptr; }

    bool HasSnapshot() const { return has_snapshot; }
    const uint256& SnapshotBaseHash() const { return snapshot_base_hash; }
    uint64_t SnapshotCoinsCount() const { return snapshot_coins_count; }
    const uint256& SnapshotCoinsHash() const { return snapshot_coins_hash; }
    bool HasSnapshotCoinsHash() const { return !snapshot_coins_hash.IsNull(); }

    void SetSnapshotInfo(const uint256& base_hash, uint64_t coins_count);
    void SetSnapshotCoinsHash(const uint256& coins_hash);
    void ClearSnapshotInfo();

    bool AllocateCoinsDB(const fs::path& db_path, bool fMemory, bool fWipe);
    void ResetCoinsDB();
    void AttachExternalCoins(CCoinsViewCache* coins);
    void UseExternalChain(CChain& chain);

private:
    std::string name;
    bool active_role;

    CChain* chain_ptr;
    std::unique_ptr<CChain> owned_chain;

    CCoinsViewCache** external_coins_tip;
    CCoinsViewCache* external_coins_fixed;
    std::unique_ptr<CCoinsViewDB> owned_coins_db;
    std::unique_ptr<CCoinsViewCache> owned_coins_tip;

    bool has_snapshot;
    uint256 snapshot_base_hash;
    uint64_t snapshot_coins_count;
    uint256 snapshot_coins_hash; // hash_serialized of assumed UTXO set
};

enum class BackgroundValidationStatus {
    NONE = 0,       // no snapshot activation
    RUNNING,        // connecting blocks toward H
    WAITING_BLOCKS, // next block not on disk yet
    COMPLETED,      // hash matched
    FAILED          // hash mismatch or connect error (fail closed)
};

void InitializeActiveChainstate();
void InitializeBackgroundChainstate();
void ShutdownChainstates();

bool IsActiveChainstateInitialized();
bool HasBackgroundChainstate();
bool IsSnapshotChainstateActive();

CChainState& ActiveChainstate();
CChainState* BackgroundChainstate();

CChain& ActiveChain();
CCoinsViewCache* ActiveCoinsTip();

bool ActivateLoadedSnapshot(std::string& error);
bool AssumeUtxoDevActivationAllowed();

/** Phase C status / progress (cs_main not required for reads of atomics-like ints after set). */
BackgroundValidationStatus GetBackgroundValidationStatus();
int GetBackgroundValidationHeight();
int GetBackgroundValidationTargetHeight();
std::string BackgroundValidationStatusString();

/**
 * Connect up to max_blocks from the parked IBD chainstate toward the snapshot base.
 * Call with cs_main held (or it will lock). Returns blocks connected this call.
 * On reaching H, hashes background UTXO set and compares to assumed snapshot hash.
 */
int StepBackgroundValidation(const CChainParams& params, int max_blocks, std::string& error);

/** Convenience: step without error string (logs on failure). */
int MaybeStepBackgroundValidation(const CChainParams& params, int max_blocks = 16);

bool IsAssumeUtxoValidated();
bool IsAssumeUtxoFailed();

#endif // DOGECOIN_NODE_CHAINSTATE_H
