// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node/chainstate.h"

#include "chainparams.h"
#include "chainparamsbase.h"
#include "coins.h"
#include "txdb.h"
#include "txmempool.h"
#include "util.h"
#include "validation.h"
#include "validationinterface.h"

#include <cassert>
#include <memory>

namespace {

CChainState* g_active_chainstate = nullptr;
/** Pre-B2 static IBD wrapper (chainActive + &pcoinsTip). Unused after activate. */
CChainState* g_ibd_global_wrapper = nullptr;
std::unique_ptr<CChainState> g_background_chainstate;
/** Snapshot chainstate after load+activate (owns coins DB). */
std::unique_ptr<CChainState> g_snapshot_chainstate;
/** Original process coins cache (global pcoinsTip at init); restored on shutdown. */
CCoinsViewCache* g_original_pcoins_tip = nullptr;
bool g_snapshot_is_active = false;

} // anon namespace

CChainState::CChainState(const std::string& nameIn, CChain& chainIn, CCoinsViewCache*& coinsTipIn)
    : name(nameIn),
      active_role(true),
      chain_ptr(&chainIn),
      owned_chain(),
      external_coins_tip(&coinsTipIn),
      external_coins_fixed(nullptr),
      owned_coins_db(),
      owned_coins_tip(),
      has_snapshot(false),
      snapshot_coins_count(0)
{
}

CChainState::CChainState(const std::string& nameIn)
    : name(nameIn),
      active_role(false),
      chain_ptr(nullptr),
      owned_chain(new CChain()),
      external_coins_tip(nullptr),
      external_coins_fixed(nullptr),
      owned_coins_db(),
      owned_coins_tip(),
      has_snapshot(false),
      snapshot_coins_count(0)
{
    chain_ptr = owned_chain.get();
}

CChainState::~CChainState()
{
    owned_coins_tip.reset();
    owned_coins_db.reset();
}

CCoinsViewCache* CChainState::CoinsTip() const
{
    if (owned_coins_tip)
        return owned_coins_tip.get();
    if (external_coins_fixed)
        return external_coins_fixed;
    if (external_coins_tip)
        return *external_coins_tip;
    return nullptr;
}

CCoinsViewCache*& CChainState::CoinsTipRef()
{
    assert(external_coins_tip != nullptr);
    return *external_coins_tip;
}

CBlockIndex* CChainState::Tip() const
{
    return chain_ptr->Tip();
}

int CChainState::Height() const
{
    return chain_ptr->Height();
}

void CChainState::SetSnapshotInfo(const uint256& base_hash, uint64_t coins_count)
{
    has_snapshot = true;
    snapshot_base_hash = base_hash;
    snapshot_coins_count = coins_count;
}

void CChainState::ClearSnapshotInfo()
{
    has_snapshot = false;
    snapshot_base_hash.SetNull();
    snapshot_coins_count = 0;
}

bool CChainState::AllocateCoinsDB(const fs::path& db_path, bool fMemory, bool fWipe)
{
    if (active_role && !has_snapshot) {
        // Allow allocate only on non-active (background load) chainstates.
        LogPrintf("Chainstate: refuse AllocateCoinsDB on active non-snapshot chainstate\n");
        return false;
    }
    ResetCoinsDB();

    const size_t nCacheSize = 8 << 20;
    try {
        owned_coins_db.reset(new CCoinsViewDB(db_path, nCacheSize, fMemory, fWipe));
        owned_coins_tip.reset(new CCoinsViewCache(owned_coins_db.get()));
    } catch (const std::exception& e) {
        LogPrintf("Chainstate: AllocateCoinsDB failed: %s\n", e.what());
        owned_coins_tip.reset();
        owned_coins_db.reset();
        return false;
    }
    external_coins_fixed = nullptr;
    LogPrintf("Chainstate: \"%s\" coins DB at %s (memory=%d wipe=%d)\n",
              name, db_path.string(), fMemory ? 1 : 0, fWipe ? 1 : 0);
    return true;
}

void CChainState::ResetCoinsDB()
{
    owned_coins_tip.reset();
    owned_coins_db.reset();
    external_coins_fixed = nullptr;
    ClearSnapshotInfo();
    if (owned_chain) {
        owned_chain->SetTip(nullptr);
    }
}

void CChainState::AttachExternalCoins(CCoinsViewCache* coins)
{
    owned_coins_tip.reset();
    owned_coins_db.reset();
    external_coins_tip = nullptr;
    external_coins_fixed = coins;
}

void CChainState::UseExternalChain(CChain& chain)
{
    chain_ptr = &chain;
    owned_chain.reset();
}

void InitializeActiveChainstate()
{
    if (g_active_chainstate) {
        return;
    }
    static CChainState single("ibd", chainActive, pcoinsTip);
    g_ibd_global_wrapper = &single;
    g_active_chainstate = &single;
    g_original_pcoins_tip = pcoinsTip;
    LogPrintf("Chainstate: active=\"%s\" (wraps chainActive/pcoinsTip)\n",
              g_active_chainstate->GetName());
}

void InitializeBackgroundChainstate()
{
    if (g_background_chainstate || g_snapshot_is_active) {
        return;
    }
    if (!g_active_chainstate) {
        InitializeActiveChainstate();
    }
    g_background_chainstate.reset(new CChainState("background"));
    LogPrintf("Chainstate: background=\"background\" idle (AssumeUTXO A3/B — load via loadtxoutset)\n");
}

void ShutdownChainstates()
{
    // If snapshot was active, pcoinsTip points into owned snapshot cache.
    // Restore original so init.cpp's delete pcoinsTip frees the IBD cache only.
    if (g_snapshot_is_active && g_original_pcoins_tip) {
        pcoinsTip = g_original_pcoins_tip;
    }
    g_snapshot_chainstate.reset();
    g_background_chainstate.reset();
    g_active_chainstate = nullptr;
    g_ibd_global_wrapper = nullptr;
    g_snapshot_is_active = false;
    g_original_pcoins_tip = nullptr;
}

bool IsActiveChainstateInitialized()
{
    return g_active_chainstate != nullptr;
}

bool HasBackgroundChainstate()
{
    return g_background_chainstate.get() != nullptr;
}

bool IsSnapshotChainstateActive()
{
    return g_snapshot_is_active;
}

CChainState& ActiveChainstate()
{
    if (!g_active_chainstate) {
        InitializeActiveChainstate();
    }
    assert(g_active_chainstate != nullptr);
    return *g_active_chainstate;
}

CChainState* BackgroundChainstate()
{
    return g_background_chainstate.get();
}

CChain& ActiveChain()
{
    return ActiveChainstate().GetChain();
}

CCoinsViewCache* ActiveCoinsTip()
{
    return ActiveChainstate().CoinsTip();
}

bool AssumeUtxoDevActivationAllowed()
{
    if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
        return true;
    }
    return GetBoolArg("-assumeutxodev", false);
}

bool ActivateLoadedSnapshot(std::string& error)
{
    error.clear();

    if (!AssumeUtxoDevActivationAllowed()) {
        error = "Snapshot activation requires -assumeutxodev=1 (always allowed on regtest). "
                "This is a dev-only Phase B2 step; Phase C will validate history in the background.";
        return false;
    }
    if (g_snapshot_is_active) {
        error = "Snapshot chainstate is already active";
        return false;
    }
    if (!g_background_chainstate) {
        error = "No background chainstate (load a snapshot first with loadtxoutset)";
        return false;
    }

    CChainState* snap = g_background_chainstate.get();
    if (!snap->HasSnapshot()) {
        error = "Background chainstate has no loaded snapshot";
        return false;
    }
    if (!snap->CoinsTip()) {
        error = "Snapshot has no coins tip";
        return false;
    }
    if (!snap->Tip()) {
        error = "Snapshot base block is not in the block index (sync headers to the snapshot height first)";
        return false;
    }

    LOCK(cs_main);

    CBlockIndex* snap_tip = snap->Tip();
    CBlockIndex* ibd_tip = chainActive.Tip();

    if (ibd_tip && snap_tip->nHeight < ibd_tip->nHeight) {
        error = strprintf(
            "Snapshot height %d is behind current tip %d; refusing to roll tip backward",
            snap_tip->nHeight, ibd_tip->nHeight);
        return false;
    }

    // Capture IBD coins before re-pointing the global.
    if (!g_original_pcoins_tip) {
        g_original_pcoins_tip = pcoinsTip;
    }
    CCoinsViewCache* ibd_coins = g_original_pcoins_tip;
    if (!ibd_coins) {
        error = "Original IBD coins tip is null";
        return false;
    }

    // Flush IBD coins to disk before parking them.
    if (pcoinsTip) {
        FlushStateToDisk();
    }

    // Invalidate mempool: inputs may not exist relative to the assumed tip.
    mempool.clear();

    // Promote snapshot unique_ptr out of the background slot.
    g_snapshot_chainstate = std::move(g_background_chainstate);
    g_snapshot_chainstate->SetName("snapshot");
    g_snapshot_chainstate->SetActiveRole(true);
    // Use process-global chainActive so ConnectBlock advances the same tip as wallet.
    g_snapshot_chainstate->UseExternalChain(chainActive);

    // Park former IBD tip + coins on a new background chainstate for Phase C.
    g_background_chainstate.reset(new CChainState("ibd"));
    g_background_chainstate->SetActiveRole(false);
    g_background_chainstate->GetChain().SetTip(ibd_tip);
    g_background_chainstate->AttachExternalCoins(ibd_coins);

    // Mirror globals for unmigrated code paths (wallet still uses chainActive/pcoinsTip).
    chainActive.SetTip(snap_tip);
    pcoinsTip = g_snapshot_chainstate->CoinsTip();
    assert(pcoinsTip != nullptr);
    pcoinsTip->SetBestBlock(snap_tip->GetBlockHash());

    g_active_chainstate = g_snapshot_chainstate.get();
    g_snapshot_is_active = true;

    // Notify UI / peers that the tip changed (fork = prior IBD tip).
    GetMainSignals().UpdatedBlockTip(snap_tip, ibd_tip, /*fInitialDownload=*/false);

    LogPrintf("Chainstate: ACTIVATED snapshot as active tip height=%d hash=%s coins=%llu "
              "(IBD tip parked at height=%d for Phase C; -assumeutxodev)\n",
              snap_tip->nHeight,
              snap_tip->GetBlockHash().ToString(),
              (unsigned long long)g_snapshot_chainstate->SnapshotCoinsCount(),
              ibd_tip ? ibd_tip->nHeight : -1);

    return true;
}
