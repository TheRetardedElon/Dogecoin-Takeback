// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node/chainstate.h"

#include "coins.h"
#include "txdb.h"
#include "util.h"
#include "validation.h"

#include <cassert>
#include <memory>

namespace {

CChainState* g_active_chainstate = nullptr;
std::unique_ptr<CChainState> g_background_chainstate;

} // anon namespace

CChainState::CChainState(const std::string& nameIn, CChain& chainIn, CCoinsViewCache*& coinsTipIn)
    : name(nameIn),
      active_role(true),
      chain_ptr(&chainIn),
      owned_chain(),
      external_coins_tip(&coinsTipIn),
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
      owned_coins_db(),
      owned_coins_tip(),
      has_snapshot(false),
      snapshot_coins_count(0)
{
    chain_ptr = owned_chain.get();
}

CChainState::~CChainState()
{
    // Destroy cache before DB (cache holds base pointer into DB).
    owned_coins_tip.reset();
    owned_coins_db.reset();
}

CCoinsViewCache* CChainState::CoinsTip() const
{
    if (external_coins_tip)
        return *external_coins_tip;
    return owned_coins_tip.get();
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
    if (active_role) {
        LogPrintf("Chainstate: refuse AllocateCoinsDB on active chainstate\n");
        return false;
    }
    ResetCoinsDB();

    // Modest default cache for background snapshot load (MiB → bytes via txdb defaults path).
    // nCoinDBCache is in bytes in init; here use 8 MiB like nMaxCoinsDBCache default units.
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
    LogPrintf("Chainstate: \"%s\" coins DB at %s (memory=%d wipe=%d)\n",
              name, db_path.string(), fMemory ? 1 : 0, fWipe ? 1 : 0);
    return true;
}

void CChainState::ResetCoinsDB()
{
    owned_coins_tip.reset();
    owned_coins_db.reset();
    ClearSnapshotInfo();
    if (owned_chain) {
        owned_chain->SetTip(nullptr);
    }
}

void InitializeActiveChainstate()
{
    if (g_active_chainstate) {
        return;
    }
    // Lifetime: process duration. Points at validation globals which outlive this.
    static CChainState single("ibd", chainActive, pcoinsTip);
    g_active_chainstate = &single;
    LogPrintf("Chainstate: active=\"%s\" (wraps chainActive/pcoinsTip)\n",
              g_active_chainstate->GetName());
}

void InitializeBackgroundChainstate()
{
    if (g_background_chainstate) {
        return;
    }
    // Ensure active exists first so getchainstates ordering is stable.
    if (!g_active_chainstate) {
        InitializeActiveChainstate();
    }
    g_background_chainstate.reset(new CChainState("background"));
    LogPrintf("Chainstate: background=\"background\" idle (owns empty CChain; no coins DB — AssumeUTXO Phase A3/B)\n");
}

void ShutdownChainstates()
{
    g_background_chainstate.reset();
    g_active_chainstate = nullptr;
}

bool IsActiveChainstateInitialized()
{
    return g_active_chainstate != nullptr;
}

bool HasBackgroundChainstate()
{
    return g_background_chainstate.get() != nullptr;
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
