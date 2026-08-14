// Copyright (c) 2026 The Dogecoin Core Pro developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_DBMIGRATE_H
#define DOGECOIN_DBMIGRATE_H

#include "dbengine.h"
#include "fs.h"

#include <stdint.h>
#include <string>

/**
 * Copy every key/value byte from src to dst.
 * Does not re-serialize or re-XOR. hash_serialized stays valid.
 * Source is left untouched. Destination must be empty / absent.
 */
bool CopyKvDirectory(const fs::path& src,
                     DbEngine srcEngine,
                     const fs::path& dst,
                     DbEngine dstEngine,
                     uint64_t& keysOut,
                     std::string& errOut);

/**
 * -migratedb=mdbx : copy datadir/chainstate and datadir/blocks/index
 * into sibling *_<name> folders.
 * With -swapdb, rename live folders aside and move the copies into place,
 * then return true so startup continues on the new engine.
 * Without -swapdb, return false after a success message (fail-closed exit).
 */
bool RunRequestedDbMigration(const fs::path& datadir, std::string& errOut);

#endif // DOGECOIN_DBMIGRATE_H
