// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_NODE_CHAINSTATE_H
#define DOGECOIN_NODE_CHAINSTATE_H

#include <string>

class CBlockIndex;
class CChain;
class CCoinsViewCache;

/**
 * A chainstate is a tip (CChain) plus the UTXO cache for that tip.
 *
 * AssumeUTXO Phase A (slice 1): a single CChainState wraps the process
 * globals chainActive / pcoinsTip. Behavior is identical to pre-abstraction
 * code. Later slices may own a second CChainState for background validation.
 *
 * New code should prefer ActiveChainstate() / ActiveChain() / ActiveCoinsTip()
 * over bare globals so dual-chainstate work does not require a second rewrite.
 */
class CChainState
{
public:
    CChainState(const std::string& nameIn, CChain& chainIn, CCoinsViewCache*& coinsTipIn);

    const std::string& GetName() const { return name; }

    CChain& GetChain() { return chain; }
    const CChain& GetChain() const { return chain; }

    CCoinsViewCache* CoinsTip() const { return coins_tip; }
    CCoinsViewCache*& CoinsTipRef() { return coins_tip; }

    CBlockIndex* Tip() const;
    int Height() const;

private:
    std::string name;
    CChain& chain;
    CCoinsViewCache*& coins_tip;
};

/**
 * Bind the active chainstate to existing validation globals.
 * Safe to call once after pcoinsTip has been constructed in AppInitMain.
 * Idempotent if already initialized.
 */
void InitializeActiveChainstate();

/** True after InitializeActiveChainstate(). */
bool IsActiveChainstateInitialized();

/**
 * Active (wallet / tip / net) chainstate.
 * Requires InitializeActiveChainstate(); asserts otherwise.
 */
CChainState& ActiveChainstate();

/** Shortcuts — same objects as chainActive / pcoinsTip in Phase A. */
CChain& ActiveChain();
CCoinsViewCache* ActiveCoinsTip();

#endif // DOGECOIN_NODE_CHAINSTATE_H
