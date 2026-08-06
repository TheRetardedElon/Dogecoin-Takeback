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
#include <vector>

class CBlockIndex;
class CChainParams;
class CCoinsViewCache;
class CCoinsViewDB;

/**
 * A chainstate is a tip (CChain) plus optional UTXO cache for that tip.
 *
 * AssumeUTXO progression:
 *  - A1–A3: dual chainstate foundation
 *  - B1/B2: load + activate snapshot tip
 *  - C1: background ConnectBlock + hash check
 *  - C2: persist/restore across restarts; fetch missing history; collapse after validated
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
    /** Open existing coins DB without wiping (C2 restore). */
    bool OpenCoinsDB(const fs::path& db_path, bool fMemory);
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
    uint256 snapshot_coins_hash;
};

enum class BackgroundValidationStatus {
    NONE = 0,
    RUNNING,
    WAITING_BLOCKS,
    COMPLETED,
    FAILED
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
/** True if -assumeutxodev or regtest. */
bool AssumeUtxoDevActivationAllowed();
/**
 * Whether activation is allowed for a snapshot at height with coins_hash.
 * Allowed if: regtest, -assumeutxodev, or height+hash matches chainparams AssumeutxoData.
 */
bool AssumeUtxoActivationAllowed(int height, const uint256& coins_hash, std::string& error);
/** Progress 0.0–1.0 of background validation (1.0 if complete/none). */
double GetAssumeUtxoValidationProgress();

/**
 * Phase C2: if assumeutxo.dat + chainstate_snapshot/ exist after normal init,
 * re-activate the snapshot tip and resume (or complete) background validation.
 * Call after InitializeActiveChainstate/InitializeBackgroundChainstate.
 */
bool MaybeRestoreAssumeUtxo(std::string& error);

/** Persist assumeutxo.dat (activate / progress / complete / fail). */
bool PersistAssumeUtxoState(std::string& error);

BackgroundValidationStatus GetBackgroundValidationStatus();
int GetBackgroundValidationHeight();
int GetBackgroundValidationTargetHeight();
std::string BackgroundValidationStatusString();

int StepBackgroundValidation(const CChainParams& params, int max_blocks, std::string& error);
int MaybeStepBackgroundValidation(const CChainParams& params, int max_blocks = 16);

bool IsAssumeUtxoValidated();
bool IsAssumeUtxoFailed();
/** True after successful validation (dual-work finished for this process/datadir). */
bool IsAssumeUtxoDualCollapsed();

/**
 * Append up to max_count historical blocks on the path to the snapshot base
 * that are missing BLOCK_HAVE_DATA (even if they are in chainActive).
 * Used by net_processing to fetch pruned/missing history for Phase C.
 */
void GetBackgroundValidationMissingBlocks(std::vector<const CBlockIndex*>& out, unsigned int max_count);

#endif // DOGECOIN_NODE_CHAINSTATE_H
