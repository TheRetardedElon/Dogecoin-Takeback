// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node/chainstate.h"

#include "chain.h"
#include "coins.h"
#include "util.h"
#include "validation.h"

#include <cassert>

namespace {

CChainState* g_active_chainstate = nullptr;

} // anon namespace

CChainState::CChainState(const std::string& nameIn, CChain& chainIn, CCoinsViewCache*& coinsTipIn)
    : name(nameIn), chain(chainIn), coins_tip(coinsTipIn)
{
}

CBlockIndex* CChainState::Tip() const
{
    return chain.Tip();
}

int CChainState::Height() const
{
    return chain.Height();
}

void InitializeActiveChainstate()
{
    if (g_active_chainstate) {
        return;
    }
    // Lifetime: process duration. Points at validation globals which outlive this.
    // coins_tip is a reference to the global pcoinsTip pointer, so later reassignment
    // (load/reindex/tests) is visible through CoinsTip().
    static CChainState single("ibd", chainActive, pcoinsTip);
    g_active_chainstate = &single;
    LogPrintf("Chainstate: active=\"%s\" (single-chainstate Phase A; AssumeUTXO dual state not yet enabled)\n",
              g_active_chainstate->GetName());
}

bool IsActiveChainstateInitialized()
{
    return g_active_chainstate != nullptr;
}

CChainState& ActiveChainstate()
{
    // Lazy bind for unit tests and any path that builds a tip before AppInitMain ends.
    if (!g_active_chainstate) {
        InitializeActiveChainstate();
    }
    assert(g_active_chainstate != nullptr);
    return *g_active_chainstate;
}

CChain& ActiveChain()
{
    return ActiveChainstate().GetChain();
}

CCoinsViewCache* ActiveCoinsTip()
{
    return ActiveChainstate().CoinsTip();
}
