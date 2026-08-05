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
 * AssumeUTXO Phase A/B progression:
 *  - A1/A2: active "ibd" wraps globals chainActive / pcoinsTip
 *  - A3: idle "background" owns an empty CChain (no coins DB)
 *  - B1: background can load a UTXO snapshot into chainstate_snapshot/
 *  - C+: background validates genesis→H; snapshot may become active tip
 *
 * Prefer ActiveChainstate() / ActiveChain() / ActiveCoinsTip() over bare globals.
 */
class CChainState
{
public:
    /** Active chainstate: non-owning refs to process globals. */
    CChainState(const std::string& nameIn, CChain& chainIn, CCoinsViewCache*& coinsTipIn);

    /**
     * Idle background chainstate: owns its CChain (empty tip).
     * coins view allocated later via AllocateCoinsDB / LoadUTXOSnapshot.
     */
    explicit CChainState(const std::string& nameIn);

    ~CChainState();

    const std::string& GetName() const { return name; }

    CChain& GetChain() { return *chain_ptr; }
    const CChain& GetChain() const { return *chain_ptr; }

    CCoinsViewCache* CoinsTip() const;
    /** Only valid for active (global-backed) chainstates. */
    CCoinsViewCache*& CoinsTipRef();

    CBlockIndex* Tip() const;
    int Height() const;

    /** True if this is the process active (wallet/net) chainstate. */
    bool IsActiveRole() const { return active_role; }
    /** True if no coins view is attached (idle background). */
    bool IsIdle() const { return CoinsTip() == nullptr; }
    /** True if this instance owns its CChain (background). */
    bool OwnsChain() const { return owned_chain.get() != nullptr; }

    /** Snapshot load bookkeeping (background only). */
    bool HasSnapshot() const { return has_snapshot; }
    const uint256& SnapshotBaseHash() const { return snapshot_base_hash; }
    uint64_t SnapshotCoinsCount() const { return snapshot_coins_count; }
    void SetSnapshotInfo(const uint256& base_hash, uint64_t coins_count);
    void ClearSnapshotInfo();

    /**
     * Allocate an owned coins DB + cache for this (background) chainstate.
     * Path is typically GetDataDir()/chainstate_snapshot.
     */
    bool AllocateCoinsDB(const fs::path& db_path, bool fMemory, bool fWipe);

    /** Tear down owned coins DB/cache (leaves chain empty; clears snapshot flags). */
    void ResetCoinsDB();

private:
    std::string name;
    bool active_role;

    CChain* chain_ptr;
    std::unique_ptr<CChain> owned_chain;

    CCoinsViewCache** external_coins_tip; // points at global pcoinsTip for active
    std::unique_ptr<CCoinsViewDB> owned_coins_db;
    std::unique_ptr<CCoinsViewCache> owned_coins_tip;

    bool has_snapshot;
    uint256 snapshot_base_hash;
    uint64_t snapshot_coins_count;
};

/**
 * Bind the active chainstate to existing validation globals.
 * Safe after pcoinsTip is constructed. Idempotent.
 */
void InitializeActiveChainstate();

/**
 * Create the idle background chainstate slot (A3).
 * Does not allocate a coins database or download blocks.
 * Idempotent.
 */
void InitializeBackgroundChainstate();

/** Drop chainstate managers (call before deleting global pcoinsTip). */
void ShutdownChainstates();

bool IsActiveChainstateInitialized();
bool HasBackgroundChainstate();

CChainState& ActiveChainstate();
/** Null if background not initialized. */
CChainState* BackgroundChainstate();

CChain& ActiveChain();
CCoinsViewCache* ActiveCoinsTip();

#endif // DOGECOIN_NODE_CHAINSTATE_H
