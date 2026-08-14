// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_BLOCKARCHIVE_H
#define DOGECOIN_BLOCKARCHIVE_H

#include "fs.h"

#include <stdint.h>
#include <string>
#include <vector>

/**
 * Cold copy of finalized blk/rev files.
 *
 * Live datadir (wallet, chainstate, active block file, block index) stays on
 * local disk. When prune is about to delete a closed blk/rev pair, we copy
 * it to -archivepath first (SHA-256 sidecar). Copy failure skips the unlink
 * so the only copy is not destroyed.
 *
 * Destination may be an operator file store (/mnt/vfs) or a desktop folder.
 * It must not be the live datadir, blocks/, or chainstate/.
 */
struct BlockArchiveInfo {
    bool enabled = false;
    std::string path;
    int files = 0;
    int sidecars = 0;
    uint64_t bytes = 0;
};

struct BlockArchiveVerify {
    int checked = 0;
    int ok = 0;
    int failed = 0;
    int missing_sidecar = 0;
    std::vector<std::string> failed_names;
};

bool InitBlockArchive(std::string& err);
fs::path GetBlockArchivePath();
bool ArchiveBlockFilePair(int nFile);
BlockArchiveInfo GetBlockArchiveInfo();
BlockArchiveVerify VerifyBlockArchive(int maxFiles = 0);

#endif // DOGECOIN_BLOCKARCHIVE_H
