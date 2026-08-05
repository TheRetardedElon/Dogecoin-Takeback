// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node/chainstate.h"

#include "chainparams.h"
#include "chainparamsbase.h"
#include "coins.h"
#include "consensus/validation.h"
#include "init.h"
#include "node/utxo_snapshot.h"
#include "primitives/block.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"
#include "validation.h"
#include "validationinterface.h"

#include <cassert>
#include <memory>

namespace {

CChainState* g_active_chainstate = nullptr;
CChainState* g_ibd_global_wrapper = nullptr;
std::unique_ptr<CChainState> g_background_chainstate;
std::unique_ptr<CChainState> g_snapshot_chainstate;
CCoinsViewCache* g_original_pcoins_tip = nullptr;
bool g_snapshot_is_active = false;

BackgroundValidationStatus g_bg_status = BackgroundValidationStatus::NONE;
int g_bg_height = -1;
int g_bg_target_height = -1;
bool g_assumeutxo_validated = false;
bool g_assumeutxo_failed = false;
bool g_dual_collapsed = false;
uint256 g_expected_coins_hash;
uint256 g_snapshot_base_hash;
uint64_t g_snapshot_coins_count = 0;

uint8_t StatusToDisk(BackgroundValidationStatus s)
{
    switch (s) {
    case BackgroundValidationStatus::RUNNING: return ASSUMEUTXO_DISK_RUNNING;
    case BackgroundValidationStatus::WAITING_BLOCKS: return ASSUMEUTXO_DISK_WAITING;
    case BackgroundValidationStatus::COMPLETED: return ASSUMEUTXO_DISK_COMPLETED;
    case BackgroundValidationStatus::FAILED: return ASSUMEUTXO_DISK_FAILED;
    default: return ASSUMEUTXO_DISK_NONE;
    }
}

BackgroundValidationStatus StatusFromDisk(uint8_t s)
{
    switch (s) {
    case ASSUMEUTXO_DISK_RUNNING: return BackgroundValidationStatus::RUNNING;
    case ASSUMEUTXO_DISK_WAITING: return BackgroundValidationStatus::WAITING_BLOCKS;
    case ASSUMEUTXO_DISK_COMPLETED: return BackgroundValidationStatus::COMPLETED;
    case ASSUMEUTXO_DISK_FAILED: return BackgroundValidationStatus::FAILED;
    default: return BackgroundValidationStatus::NONE;
    }
}

bool PersistAssumeUtxoStateUnlocked(std::string& error)
{
    if (!g_snapshot_is_active && !AssumeUtxoDiskStateExists()) {
        return true;
    }
    AssumeUtxoDiskState st;
    st.base_blockhash = g_snapshot_base_hash;
    st.coins_hash = g_expected_coins_hash;
    st.coins_count = g_snapshot_coins_count;
    st.status = StatusToDisk(g_bg_status);
    st.bg_height = g_bg_height;
    if (g_snapshot_chainstate) {
        if (g_snapshot_chainstate->HasSnapshot()) {
            st.coins_count = g_snapshot_chainstate->SnapshotCoinsCount();
        }
    }
    return WriteAssumeUtxoDiskState(st, error);
}

void FailBackgroundValidation(const std::string& reason)
{
    g_bg_status = BackgroundValidationStatus::FAILED;
    g_assumeutxo_failed = true;
    g_assumeutxo_validated = false;
    g_dual_collapsed = false;
    LogPrintf("*** ASSUMEUTXO BACKGROUND VALIDATION FAILED: %s\n", reason);
    LogPrintf("*** Snapshot UTXO set does not match fully validated history. "
              "Shutting down. Delete chainstate_snapshot and assumeutxo.dat, then restart "
              "with -reindex-chainstate or without the snapshot.\n");
    uiInterface.ThreadSafeMessageBox(
        _("AssumeUTXO background validation failed: the loaded snapshot does not match "
          "the fully validated chain. See debug.log. The node will shut down."),
        "", CClientUIInterface::MSG_ERROR);
    std::string perr;
    PersistAssumeUtxoStateUnlocked(perr);
    StartShutdown();
}

void CompleteBackgroundValidation()
{
    g_bg_status = BackgroundValidationStatus::COMPLETED;
    g_assumeutxo_validated = true;
    g_assumeutxo_failed = false;
    // Soft collapse: dual work finished; background may remain on disk for diagnostics
    // but we stop stepping and treat the node as fully validated from genesis→H.
    g_dual_collapsed = true;
    LogPrintf("Chainstate: AssumeUTXO background validation COMPLETED at height %d "
              "(coins hash matches assumed snapshot %s). Dual validation collapsed.\n",
              g_bg_height, g_expected_coins_hash.ToString());
    std::string perr;
    if (!PersistAssumeUtxoStateUnlocked(perr)) {
        LogPrintf("Chainstate: warning: failed to persist assumeutxo.dat after complete: %s\n", perr);
    }
}

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

void CChainState::SetSnapshotCoinsHash(const uint256& coins_hash)
{
    snapshot_coins_hash = coins_hash;
}

void CChainState::ClearSnapshotInfo()
{
    has_snapshot = false;
    snapshot_base_hash.SetNull();
    snapshot_coins_count = 0;
    snapshot_coins_hash.SetNull();
}

bool CChainState::AllocateCoinsDB(const fs::path& db_path, bool fMemory, bool fWipe)
{
    if (active_role && !has_snapshot) {
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

bool CChainState::OpenCoinsDB(const fs::path& db_path, bool fMemory)
{
    ResetCoinsDB();
    const size_t nCacheSize = 8 << 20;
    try {
        owned_coins_db.reset(new CCoinsViewDB(db_path, nCacheSize, fMemory, /*fWipe=*/false));
        owned_coins_tip.reset(new CCoinsViewCache(owned_coins_db.get()));
    } catch (const std::exception& e) {
        LogPrintf("Chainstate: OpenCoinsDB failed: %s\n", e.what());
        owned_coins_tip.reset();
        owned_coins_db.reset();
        return false;
    }
    external_coins_fixed = nullptr;
    LogPrintf("Chainstate: \"%s\" opened coins DB at %s (memory=%d)\n",
              name, db_path.string(), fMemory ? 1 : 0);
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
    if (g_snapshot_is_active) {
        std::string perr;
        // Flush snapshot tip before tear-down so restart can restore.
        if (g_snapshot_chainstate && g_snapshot_chainstate->CoinsTip()) {
            g_snapshot_chainstate->CoinsTip()->Flush();
        }
        PersistAssumeUtxoStateUnlocked(perr);
        if (g_original_pcoins_tip) {
            pcoinsTip = g_original_pcoins_tip;
        }
    }
    g_snapshot_chainstate.reset();
    g_background_chainstate.reset();
    g_active_chainstate = nullptr;
    g_ibd_global_wrapper = nullptr;
    g_snapshot_is_active = false;
    g_original_pcoins_tip = nullptr;
    g_bg_status = BackgroundValidationStatus::NONE;
    g_bg_height = -1;
    g_bg_target_height = -1;
    g_assumeutxo_validated = false;
    g_assumeutxo_failed = false;
    g_dual_collapsed = false;
    g_expected_coins_hash.SetNull();
    g_snapshot_base_hash.SetNull();
    g_snapshot_coins_count = 0;
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

BackgroundValidationStatus GetBackgroundValidationStatus()
{
    return g_bg_status;
}

int GetBackgroundValidationHeight()
{
    return g_bg_height;
}

int GetBackgroundValidationTargetHeight()
{
    return g_bg_target_height;
}

std::string BackgroundValidationStatusString()
{
    switch (g_bg_status) {
    case BackgroundValidationStatus::NONE: return "none";
    case BackgroundValidationStatus::RUNNING: return "running";
    case BackgroundValidationStatus::WAITING_BLOCKS: return "waiting_blocks";
    case BackgroundValidationStatus::COMPLETED: return "completed";
    case BackgroundValidationStatus::FAILED: return "failed";
    }
    return "unknown";
}

bool IsAssumeUtxoValidated()
{
    return g_assumeutxo_validated;
}

bool IsAssumeUtxoFailed()
{
    return g_assumeutxo_failed;
}

bool IsAssumeUtxoDualCollapsed()
{
    return g_dual_collapsed;
}

bool PersistAssumeUtxoState(std::string& error)
{
    return PersistAssumeUtxoStateUnlocked(error);
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
    if (!snap->HasSnapshotCoinsHash()) {
        error = "Snapshot coins hash missing (reload snapshot with a current build)";
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

    if (!g_original_pcoins_tip) {
        g_original_pcoins_tip = pcoinsTip;
    }
    CCoinsViewCache* ibd_coins = g_original_pcoins_tip;
    if (!ibd_coins) {
        error = "Original IBD coins tip is null";
        return false;
    }

    if (pcoinsTip) {
        FlushStateToDisk();
    }

    mempool.clear();

    g_expected_coins_hash = snap->SnapshotCoinsHash();
    g_snapshot_base_hash = snap->SnapshotBaseHash();
    g_snapshot_coins_count = snap->SnapshotCoinsCount();
    g_bg_target_height = snap_tip->nHeight;
    g_bg_height = ibd_tip ? ibd_tip->nHeight : -1;
    g_bg_status = BackgroundValidationStatus::RUNNING;
    g_assumeutxo_validated = false;
    g_assumeutxo_failed = false;
    g_dual_collapsed = false;

    g_snapshot_chainstate = std::move(g_background_chainstate);
    g_snapshot_chainstate->SetName("snapshot");
    g_snapshot_chainstate->SetActiveRole(true);
    g_snapshot_chainstate->UseExternalChain(chainActive);

    g_background_chainstate.reset(new CChainState("ibd"));
    g_background_chainstate->SetActiveRole(false);
    g_background_chainstate->GetChain().SetTip(ibd_tip);
    g_background_chainstate->AttachExternalCoins(ibd_coins);

    chainActive.SetTip(snap_tip);
    pcoinsTip = g_snapshot_chainstate->CoinsTip();
    assert(pcoinsTip != nullptr);
    pcoinsTip->SetBestBlock(snap_tip->GetBlockHash());

    g_active_chainstate = g_snapshot_chainstate.get();
    g_snapshot_is_active = true;

    GetMainSignals().UpdatedBlockTip(snap_tip, ibd_tip, /*fInitialDownload=*/false);

    LogPrintf("Chainstate: ACTIVATED snapshot as active tip height=%d hash=%s coins=%llu "
              "(IBD tip parked at height=%d; Phase C background validation RUNNING; expected coins hash=%s)\n",
              snap_tip->nHeight,
              snap_tip->GetBlockHash().ToString(),
              (unsigned long long)g_snapshot_chainstate->SnapshotCoinsCount(),
              ibd_tip ? ibd_tip->nHeight : -1,
              g_expected_coins_hash.ToString());

    // If already at target (snapshot of current tip), complete immediately.
    if (g_bg_height >= g_bg_target_height) {
        std::string step_err;
        if (g_bg_height == g_bg_target_height && ibd_tip &&
            ibd_tip->GetBlockHash() == g_snapshot_base_hash) {
            CCoinsViewCache* bg_coins = g_background_chainstate->CoinsTip();
            if (bg_coins) {
                bg_coins->Flush();
                uint256 got;
                if (ComputeCoinsHashSerialized(bg_coins, got, step_err)) {
                    if (got == g_expected_coins_hash) {
                        CompleteBackgroundValidation();
                    } else {
                        FailBackgroundValidation(strprintf(
                            "UTXO hash mismatch at height %d (got %s, expected %s)",
                            g_bg_height, got.ToString(), g_expected_coins_hash.ToString()));
                    }
                }
            }
        }
    }

    std::string perr;
    if (!PersistAssumeUtxoStateUnlocked(perr)) {
        LogPrintf("Chainstate: warning: failed to write assumeutxo.dat: %s\n", perr);
    }

    return true;
}

bool MaybeRestoreAssumeUtxo(std::string& error)
{
    error.clear();
    if (g_snapshot_is_active) {
        return true;
    }
    if (!AssumeUtxoDiskStateExists()) {
        return true;
    }

    fs::path snap_dir = GetDataDir() / SNAPSHOT_CHAINSTATE_DIR;
    if (!fs::exists(snap_dir)) {
        LogPrintf("Chainstate: assumeutxo.dat present but %s missing; removing stale state file\n",
                  SNAPSHOT_CHAINSTATE_DIR);
        RemoveAssumeUtxoDiskState();
        return true;
    }

    AssumeUtxoDiskState st;
    if (!ReadAssumeUtxoDiskState(st, error)) {
        return false;
    }
    if (st.base_blockhash.IsNull() || st.coins_hash.IsNull()) {
        error = "assumeutxo.dat has null base or coins hash";
        return false;
    }
    if (st.status == ASSUMEUTXO_DISK_FAILED) {
        error = "Previous AssumeUTXO session failed validation; remove assumeutxo.dat and "
                "chainstate_snapshot to recover, or reindex";
        return false;
    }

    // Restore does not re-check -assumeutxodev: the operator already activated once.
    LOCK(cs_main);

    if (!g_active_chainstate) {
        InitializeActiveChainstate();
    }
    if (!g_background_chainstate) {
        InitializeBackgroundChainstate();
    }

    BlockMap::iterator it_base = mapBlockIndex.find(st.base_blockhash);
    if (it_base == mapBlockIndex.end() || !it_base->second) {
        error = strprintf("Snapshot base %s not in block index", st.base_blockhash.ToString());
        return false;
    }
    CBlockIndex* base_index = it_base->second;

    // Open snapshot coins (no wipe).
    std::unique_ptr<CChainState> snap(new CChainState("snapshot"));
    if (!snap->OpenCoinsDB(snap_dir, /*fMemory=*/false)) {
        error = "Failed to open chainstate_snapshot database";
        return false;
    }
    CCoinsViewCache* snap_coins = snap->CoinsTip();
    if (!snap_coins) {
        error = "Snapshot coins tip null after open";
        return false;
    }

    uint256 snap_best = snap_coins->GetBestBlock();
    if (snap_best.IsNull()) {
        error = "chainstate_snapshot has null best block";
        return false;
    }
    BlockMap::iterator it_tip = mapBlockIndex.find(snap_best);
    if (it_tip == mapBlockIndex.end() || !it_tip->second) {
        error = strprintf("Snapshot tip %s not in block index", snap_best.ToString());
        return false;
    }
    CBlockIndex* snap_tip = it_tip->second;

    // Snapshot tip must be base or a descendant of base.
    if (snap_tip->GetAncestor(base_index->nHeight) != base_index) {
        error = "Snapshot coins tip is not on the assumed base chain";
        return false;
    }

    CBlockIndex* ibd_tip = chainActive.Tip();
    if (!g_original_pcoins_tip) {
        g_original_pcoins_tip = pcoinsTip;
    }
    CCoinsViewCache* ibd_coins = g_original_pcoins_tip;
    if (!ibd_coins) {
        error = "IBD coins tip null during restore";
        return false;
    }

    // Derive background tip from IBD coins bestblock when possible.
    CBlockIndex* bg_tip = ibd_tip;
    uint256 ibd_best = ibd_coins->GetBestBlock();
    if (!ibd_best.IsNull()) {
        BlockMap::iterator it_ibd = mapBlockIndex.find(ibd_best);
        if (it_ibd != mapBlockIndex.end()) {
            bg_tip = it_ibd->second;
        }
    }

    mempool.clear();

    g_expected_coins_hash = st.coins_hash;
    g_snapshot_base_hash = st.base_blockhash;
    g_snapshot_coins_count = st.coins_count;
    g_bg_target_height = base_index->nHeight;
    g_bg_height = bg_tip ? bg_tip->nHeight : st.bg_height;
    g_bg_status = StatusFromDisk(st.status);
    if (g_bg_status == BackgroundValidationStatus::NONE) {
        g_bg_status = BackgroundValidationStatus::RUNNING;
    }
    g_assumeutxo_validated = (g_bg_status == BackgroundValidationStatus::COMPLETED);
    g_assumeutxo_failed = false;
    g_dual_collapsed = g_assumeutxo_validated;

    snap->SetSnapshotInfo(st.base_blockhash, st.coins_count);
    snap->SetSnapshotCoinsHash(st.coins_hash);
    snap->SetActiveRole(true);
    snap->UseExternalChain(chainActive);

    // Idle background slot may exist — replace with parked IBD.
    g_background_chainstate.reset(new CChainState("ibd"));
    g_background_chainstate->SetActiveRole(false);
    g_background_chainstate->GetChain().SetTip(bg_tip);
    g_background_chainstate->AttachExternalCoins(ibd_coins);

    g_snapshot_chainstate = std::move(snap);
    chainActive.SetTip(snap_tip);
    pcoinsTip = g_snapshot_chainstate->CoinsTip();
    g_active_chainstate = g_snapshot_chainstate.get();
    g_snapshot_is_active = true;

    GetMainSignals().UpdatedBlockTip(snap_tip, ibd_tip, /*fInitialDownload=*/false);

    LogPrintf("Chainstate: RESTORED assumeutxo snapshot tip height=%d hash=%s base_height=%d "
              "bg_height=%d status=%s validated=%d\n",
              snap_tip->nHeight,
              snap_tip->GetBlockHash().ToString(),
              g_bg_target_height,
              g_bg_height,
              BackgroundValidationStatusString(),
              g_assumeutxo_validated ? 1 : 0);

    if (g_bg_status == BackgroundValidationStatus::COMPLETED) {
        g_dual_collapsed = true;
    } else if (g_bg_height >= g_bg_target_height && bg_tip &&
               bg_tip->GetBlockHash() == g_snapshot_base_hash) {
        // Resume completed if IBD coins already match assumed set.
        std::string step_err;
        CCoinsViewCache* bg_coins = g_background_chainstate->CoinsTip();
        uint256 got;
        if (bg_coins && bg_coins->Flush() &&
            ComputeCoinsHashSerialized(bg_coins, got, step_err)) {
            if (got == g_expected_coins_hash) {
                CompleteBackgroundValidation();
            }
        }
    }

    return true;
}

void GetBackgroundValidationMissingBlocks(std::vector<const CBlockIndex*>& out, unsigned int max_count)
{
    out.clear();
    if (max_count == 0 || !g_snapshot_is_active) {
        return;
    }
    if (g_bg_status != BackgroundValidationStatus::RUNNING &&
        g_bg_status != BackgroundValidationStatus::WAITING_BLOCKS) {
        return;
    }
    if (g_snapshot_base_hash.IsNull()) {
        return;
    }

    AssertLockHeld(cs_main);

    BlockMap::iterator it = mapBlockIndex.find(g_snapshot_base_hash);
    if (it == mapBlockIndex.end() || !it->second) {
        return;
    }
    CBlockIndex* target = it->second;
    int start = g_bg_height + 1;
    if (start < 0) {
        start = 0;
    }
    for (int h = start; h <= target->nHeight && out.size() < max_count; h++) {
        CBlockIndex* pindex = target->GetAncestor(h);
        if (!pindex) {
            break;
        }
        if (!(pindex->nStatus & BLOCK_HAVE_DATA)) {
            out.push_back(pindex);
        }
    }
}

int StepBackgroundValidation(const CChainParams& params, int max_blocks, std::string& error)
{
    error.clear();
    if (max_blocks <= 0) {
        return 0;
    }
    if (!g_snapshot_is_active) {
        error = "Snapshot chainstate is not active";
        return 0;
    }
    if (g_bg_status == BackgroundValidationStatus::COMPLETED) {
        return 0;
    }
    if (g_bg_status == BackgroundValidationStatus::FAILED) {
        error = "Background validation already failed";
        return 0;
    }
    if (g_bg_status != BackgroundValidationStatus::RUNNING &&
        g_bg_status != BackgroundValidationStatus::WAITING_BLOCKS) {
        error = "Background validation is not running";
        return 0;
    }

    LOCK(cs_main);

    CChainState* bg = g_background_chainstate.get();
    if (!bg || !bg->CoinsTip()) {
        error = "Background chainstate or coins missing";
        FailBackgroundValidation(error);
        return 0;
    }

    BlockMap::iterator it = mapBlockIndex.find(g_snapshot_base_hash);
    if (it == mapBlockIndex.end() || !it->second) {
        error = "Snapshot base block not in block index";
        FailBackgroundValidation(error);
        return 0;
    }
    CBlockIndex* target = it->second;

    CCoinsViewCache* view = bg->CoinsTip();
    int connected = 0;

    for (int i = 0; i < max_blocks; i++) {
        if (ShutdownRequested()) {
            break;
        }

        CBlockIndex* bg_tip = bg->Tip();
        int bg_height = bg_tip ? bg_tip->nHeight : -1;
        g_bg_height = bg_height;

        if (bg_height >= target->nHeight) {
            // Finished connecting — verify UTXO hash.
            if (!bg_tip || bg_tip->GetBlockHash() != g_snapshot_base_hash) {
                error = strprintf("Background tip hash mismatch at height %d", bg_height);
                FailBackgroundValidation(error);
                return connected;
            }
            if (!view->Flush()) {
                error = "Failed to flush background coins before hash check";
                FailBackgroundValidation(error);
                return connected;
            }
            uint256 got;
            if (!ComputeCoinsHashSerialized(view, got, error)) {
                FailBackgroundValidation(error);
                return connected;
            }
            if (got != g_expected_coins_hash) {
                error = strprintf("UTXO hash mismatch at height %d (got %s, expected %s)",
                                  bg_height, got.ToString(), g_expected_coins_hash.ToString());
                FailBackgroundValidation(error);
                return connected;
            }
            CompleteBackgroundValidation();
            return connected;
        }

        const int next_height = bg_height + 1;
        CBlockIndex* pindex_next = target->GetAncestor(next_height);
        if (!pindex_next) {
            error = strprintf("Cannot find ancestor at height %d of snapshot base", next_height);
            FailBackgroundValidation(error);
            return connected;
        }

        // Must extend background tip.
        if (bg_tip) {
            if (pindex_next->pprev != bg_tip) {
                error = strprintf("Background chain broken at height %d (next pprev mismatch)", next_height);
                FailBackgroundValidation(error);
                return connected;
            }
        } else {
            // Empty background tip: next must be genesis.
            if (pindex_next->pprev != nullptr) {
                error = "Background tip empty but next block is not genesis";
                FailBackgroundValidation(error);
                return connected;
            }
        }

        if (!(pindex_next->nStatus & BLOCK_HAVE_DATA)) {
            g_bg_status = BackgroundValidationStatus::WAITING_BLOCKS;
            error = strprintf("Block data missing at height %d (%s); waiting for download",
                              next_height, pindex_next->GetBlockHash().ToString());
            LogPrint("assumeutxo", "Background validation: %s\n", error);
            std::string perr;
            PersistAssumeUtxoStateUnlocked(perr);
            return connected;
        }

        CBlock block;
        if (!ReadBlockFromDisk(block, pindex_next, params.GetConsensus(pindex_next->nHeight))) {
            error = strprintf("Failed to read block %s from disk", pindex_next->GetBlockHash().ToString());
            FailBackgroundValidation(error);
            return connected;
        }

        // Coins view must be at pprev.
        uint256 expected_prev = pindex_next->pprev ? pindex_next->pprev->GetBlockHash() : uint256();
        if (view->GetBestBlock() != expected_prev) {
            // Try to set if empty genesis case
            if (view->GetBestBlock().IsNull() && expected_prev.IsNull()) {
                // ok for genesis
            } else {
                error = strprintf("Background coins bestblock %s != expected prev %s",
                                  view->GetBestBlock().ToString(), expected_prev.ToString());
                FailBackgroundValidation(error);
                return connected;
            }
        }

        CValidationState state;
        if (!ConnectBlock(block, state, pindex_next, *view, params, /*fJustCheck=*/false)) {
            error = strprintf("ConnectBlock failed at height %d: %s",
                              next_height, FormatStateMessage(state));
            FailBackgroundValidation(error);
            return connected;
        }

        bg->GetChain().SetTip(pindex_next);
        g_bg_height = pindex_next->nHeight;
        g_bg_status = BackgroundValidationStatus::RUNNING;
        ++connected;

        // Periodic flush of background coins to disk (IBD LevelDB).
        if (connected % 100 == 0) {
            if (!view->Flush()) {
                error = "Background coins flush failed during validation";
                FailBackgroundValidation(error);
                return connected;
            }
            std::string perr;
            PersistAssumeUtxoStateUnlocked(perr);
            LogPrintf("AssumeUTXO background validation: height %d / %d (%.1f%%)\n",
                      g_bg_height, g_bg_target_height,
                      g_bg_target_height > 0 ? (100.0 * g_bg_height / g_bg_target_height) : 0.0);
        }
    }

    // Check if we landed exactly on target this batch.
    if (bg->Tip() && bg->Tip()->nHeight >= g_bg_target_height) {
        std::string err2;
        // Recurse once with 0 remaining intent — just run completion path
        int dummy_max = 1;
        // Call completion by one more iteration-less check:
        if (bg->Tip()->GetBlockHash() == g_snapshot_base_hash) {
            if (!view->Flush()) {
                FailBackgroundValidation("Failed to flush background coins before final hash check");
                return connected;
            }
            uint256 got;
            if (!ComputeCoinsHashSerialized(view, got, err2)) {
                FailBackgroundValidation(err2);
                return connected;
            }
            if (got != g_expected_coins_hash) {
                FailBackgroundValidation(strprintf(
                    "UTXO hash mismatch at height %d (got %s, expected %s)",
                    bg->Tip()->nHeight, got.ToString(), g_expected_coins_hash.ToString()));
                return connected;
            }
            CompleteBackgroundValidation();
        }
    }

    return connected;
}

int MaybeStepBackgroundValidation(const CChainParams& params, int max_blocks)
{
    if (!g_snapshot_is_active) {
        return 0;
    }
    if (g_bg_status != BackgroundValidationStatus::RUNNING &&
        g_bg_status != BackgroundValidationStatus::WAITING_BLOCKS) {
        return 0;
    }
    std::string error;
    int n = StepBackgroundValidation(params, max_blocks, error);
    if (!error.empty() && g_bg_status == BackgroundValidationStatus::WAITING_BLOCKS) {
        // missing blocks is not fatal
        return n;
    }
    if (!error.empty() && g_bg_status == BackgroundValidationStatus::FAILED) {
        LogPrintf("MaybeStepBackgroundValidation: %s\n", error);
    }
    return n;
}
