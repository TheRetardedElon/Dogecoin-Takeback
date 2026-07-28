// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_MEMESTREAMPUBLISHKEY_H
#define DOGECOIN_QT_MEMESTREAMPUBLISHKEY_H

#include <string>

/**
 * Built-in MemeStream publish material (obfuscated at rest in the binary).
 *
 * This is *not* cryptographic secret storage. Anyone with the binary can reverse
 * the reconstruction. Goals:
 *  - No single plaintext key string in source or in a trivial `strings` hit
 *  - Still ship a Core-default publish path without requiring -memestreamkey
 *
 * Override anytime with -memestreamkey= or dogecoin.conf memestreamkey=.
 */
std::string GetBuiltInMemeStreamPublishKey();

/** True if built-in material is present (always true when compiled in). */
bool HasBuiltInMemeStreamPublishKey();

#endif // DOGECOIN_QT_MEMESTREAMPUBLISHKEY_H
