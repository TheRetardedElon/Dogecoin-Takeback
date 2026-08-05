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
class CCoinsViewCache;
class CCoinsViewDB;

/**
 * A chainstate is a tip (CChain) plus optional UTXO cache for that tip.
 *
 * AssumeUTXO progression:
 *  - A1/A2: active "ibd" wraps globals chainActive / pcoinsTip
 *  - A3: idle "background" owns an empty CChain
 *  - B1: background loads a UTXO snapshot into chainstate_snapshot/
 *  - B2: ActivateLoadedSnapshot() promotes snapshot to active (wallet tip);
 *        former IBD tip preserved on background for Phase C
 *
 * Prefer ActiveChainstate() / ActiveChain() / ActiveCoinsTip() over bare globals.
 * After B2, chainActive and pcoinsTip are mirrored to the snapshot so unmigrated
 * wallet/validation code sees the assumed tip.
 */
class CChainState
{
public:
    /** Active IBD chainstate: non-owning refs to process globals. */
    CChainState(const std::string& nameIn, CChain& chainIn, CCoinsViewCache*& coinsTipIn);

    /**
     * Owned chainstate (background / snapshot): owns its CChain.
     * Coins may be allocated later via AllocateCoinsDB.
     */
    explicit CChainState(const std::string& nameIn);

    ~CChainState();

    const std::string& GetName() const { return name; }
    void SetName(const std::string& nameIn) { name = nameIn; }

    CChain& GetChain() { return *chain_ptr; }
    const CChain& GetChain() const { return *chain_ptr; }

    CCoinsViewCache* CoinsTip() const;
    /** Only valid for global-backed chainstates (external double-pointer). */
    CCoinsViewCache*& CoinsTipRef();

    CBlockIndex* Tip() const;
    int Height() const;

    bool IsActiveRole() const { return active_role; }
    void SetActiveRole(bool active) { active_role = active; }

    /** True if no coins view is attached. */
    bool IsIdle() const { return CoinsTip() == nullptr; }
    bool OwnsChain() const { return owned_chain.get() != nullptr; }

    bool HasSnapshot() const { return has_snapshot; }
    const uint256& SnapshotBaseHash() const { return snapshot_base_hash; }
    uint64_t SnapshotCoinsCount() const { return snapshot_coins_count; }
    void SetSnapshotInfo(const uint256& base_hash, uint64_t coins_count);
    void ClearSnapshotInfo();

    bool AllocateCoinsDB(const fs::path& db_path, bool fMemory, bool fWipe);
    void ResetCoinsDB();

    /**
     * Point this chainstate at an external coins cache without ownership
     * (used to park the pre-activation IBD cache on background after B2).
     */
    void AttachExternalCoins(CCoinsViewCache* coins);

    /**
     * After activation, rebind this chainstate to process-global chainActive
     * so ConnectBlock / ActivateBestChain update the same tip wallet sees.
     */
    void UseExternalChain(CChain& chain);

private:
    std::string name;
    bool active_role;

    CChain* chain_ptr;
    std::unique_ptr<CChain> owned_chain;

    /** Points at global pcoinsTip variable (IBD wrapper before B2). */
    CCoinsViewCache** external_coins_tip;
    /** Fixed non-owning coins pointer (IBD parked on background after B2). */
    CCoinsViewCache* external_coins_fixed;
    std::unique_ptr<CCoinsViewDB> owned_coins_db;
    std::unique_ptr<CCoinsViewCache> owned_coins_tip;

    bool has_snapshot;
    uint256 snapshot_base_hash;
    uint64_t snapshot_coins_count;
};

void InitializeActiveChainstate();
void InitializeBackgroundChainstate();
void ShutdownChainstates();

bool IsActiveChainstateInitialized();
bool HasBackgroundChainstate();
/** True after ActivateLoadedSnapshot succeeded this process. */
bool IsSnapshotChainstateActive();

CChainState& ActiveChainstate();
CChainState* BackgroundChainstate();

CChain& ActiveChain();
CCoinsViewCache* ActiveCoinsTip();

/**
 * Phase B2: promote a loaded background snapshot to the active tip.
 *
 * Requires:
 *  - background has snapshot with tip in block index
 *  - -assumeutxodev=1, or regtest (always allowed)
 *  - snapshot height >= current chainActive height
 *
 * Effects:
 *  - chainActive + pcoinsTip mirrored to snapshot (wallet/validation)
 *  - mempool cleared
 *  - former IBD tip/coins preserved on background as "ibd"
 *  - does NOT start background validation (Phase C)
 */
bool ActivateLoadedSnapshot(std::string& error);

/** True if activation is allowed (regtest or -assumeutxodev). */
bool AssumeUtxoDevActivationAllowed();

#endif // DOGECOIN_NODE_CHAINSTATE_H
