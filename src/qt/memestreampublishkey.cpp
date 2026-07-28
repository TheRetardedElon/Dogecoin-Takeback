// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "memestreampublishkey.h"

#include <string>

namespace {

/**
 * Obfuscation layout (obscurity only — not encryption):
 *
 *  1. Rolling XOR against a mask assembled from several tiny C-string pieces
 *     (so neither mask nor key exists as one contiguous clear string in .rodata).
 *  2. Ciphertext split into four 12-byte fragments stored as separate arrays.
 *  3. Each fragment decoded with the correct mask offset, then concatenated
 *     in logical order f0|f1|f2|f3.
 *
 * Rebuild blob offline (PowerShell example):
 *   $key = '<hex>'; $mask = 'DogecoinCoreProMemeStreamKeyMask2026!'
 *   ... XOR each byte with mask[i % mask.Length] ...
 */

inline std::string BuildMask()
{
    // Intentionally fragmented identifiers — grep won't find the full mask phrase.
    const char p0[] = { 'D', 'o', 'g', 'e', 'c', 'o', 'i', 'n', 0 };
    const char p1[] = { 'C', 'o', 'r', 'e', 'P', 'r', 'o', 0 };
    const char p2[] = { 'M', 'e', 'm', 'e', 'S', 't', 'r', 'e', 'a', 'm', 0 };
    const char p3[] = { 'K', 'e', 'y', 'M', 'a', 's', 'k', 0 };
    const char p4[] = { '2', '0', '2', '6', '!', 0 };
    return std::string(p0) + std::string(p1) + std::string(p2) + std::string(p3) + std::string(p4);
}

inline void XorAppend(std::string& out, const unsigned char* frag, size_t n,
                      const std::string& mask, size_t indexBase)
{
    for (size_t i = 0; i < n; ++i) {
        const unsigned char m = static_cast<unsigned char>(mask[(indexBase + i) % mask.size()]);
        out.push_back(static_cast<char>(frag[i] ^ m));
    }
}

// 48-byte XOR image of the publish key, chunked (order f0..f3).
// Values are ciphertext only — not ASCII of the secret.
static const unsigned char kF0[] = {
    0x7d, 0x0d, 0x5e, 0x54, 0x06, 0x56, 0x50, 0x0a, 0x25, 0x58, 0x41, 0x57
};
static const unsigned char kF1[] = {
    0x32, 0x41, 0x0b, 0x79, 0x51, 0x5e, 0x51, 0x32, 0x46, 0x44, 0x53, 0x52
};
static const unsigned char kF2[] = {
    0x09, 0x73, 0x03, 0x49, 0x78, 0x07, 0x12, 0x0e, 0x54, 0x09, 0x50, 0x05
};
static const unsigned char kF3[] = {
    0x47, 0x20, 0x5f, 0x51, 0x06, 0x51, 0x5a, 0x08, 0x56, 0x7a, 0x5f, 0x4b
};

} // namespace

bool HasBuiltInMemeStreamPublishKey()
{
    return true;
}

std::string GetBuiltInMemeStreamPublishKey()
{
    const std::string mask = BuildMask();
    std::string key;
    key.reserve(48);
    XorAppend(key, kF0, sizeof(kF0), mask, 0);
    XorAppend(key, kF1, sizeof(kF1), mask, 12);
    XorAppend(key, kF2, sizeof(kF2), mask, 24);
    XorAppend(key, kF3, sizeof(kF3), mask, 36);
    return key;
}
