// Copyright (c) 2019-2022 The Bitcoin Core developers
// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asmap.h"

#include "crypto/common.h"
#include "netaddress.h"
#include "util.h"

#include <cassert>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

std::vector<bool> g_asmap;

constexpr uint32_t INVALID = 0xFFFFFFFF;

uint32_t DecodeBits(std::vector<bool>::const_iterator& bitpos, const std::vector<bool>::const_iterator& endpos, uint8_t minval, const std::vector<uint8_t>& bit_sizes)
{
    uint32_t val = minval;
    bool bit;
    for (std::vector<uint8_t>::const_iterator bit_sizes_it = bit_sizes.begin();
         bit_sizes_it != bit_sizes.end(); ++bit_sizes_it) {
        if (bit_sizes_it + 1 != bit_sizes.end()) {
            if (bitpos == endpos) break;
            bit = *bitpos;
            bitpos++;
        } else {
            bit = 0;
        }
        if (bit) {
            val += (1 << *bit_sizes_it);
        } else {
            for (int b = 0; b < *bit_sizes_it; b++) {
                if (bitpos == endpos) return INVALID;
                bit = *bitpos;
                bitpos++;
                val += bit << (*bit_sizes_it - 1 - b);
            }
            return val;
        }
    }
    return INVALID;
}

enum class Instruction : uint32_t {
    RETURN = 0,
    JUMP = 1,
    MATCH = 2,
    DEFAULT = 3,
};

const std::vector<uint8_t> TYPE_BIT_SIZES{0, 0, 1};
Instruction DecodeType(std::vector<bool>::const_iterator& bitpos, const std::vector<bool>::const_iterator& endpos)
{
    return Instruction(DecodeBits(bitpos, endpos, 0, TYPE_BIT_SIZES));
}

const std::vector<uint8_t> ASN_BIT_SIZES{15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
uint32_t DecodeASN(std::vector<bool>::const_iterator& bitpos, const std::vector<bool>::const_iterator& endpos)
{
    return DecodeBits(bitpos, endpos, 1, ASN_BIT_SIZES);
}

const std::vector<uint8_t> MATCH_BIT_SIZES{1, 2, 3, 4, 5, 6, 7, 8};
uint32_t DecodeMatch(std::vector<bool>::const_iterator& bitpos, const std::vector<bool>::const_iterator& endpos)
{
    return DecodeBits(bitpos, endpos, 2, MATCH_BIT_SIZES);
}

const std::vector<uint8_t> JUMP_BIT_SIZES{5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
uint32_t DecodeJump(std::vector<bool>::const_iterator& bitpos, const std::vector<bool>::const_iterator& endpos)
{
    return DecodeBits(bitpos, endpos, 17, JUMP_BIT_SIZES);
}

} // anon namespace

uint32_t InterpretASMap(const std::vector<bool>& asmap, const std::vector<bool>& ip)
{
    std::vector<bool>::const_iterator pos = asmap.begin();
    const std::vector<bool>::const_iterator endpos = asmap.end();
    uint8_t bits = ip.size();
    uint32_t default_asn = 0;
    uint32_t jump, match, matchlen;
    Instruction opcode;
    while (pos != endpos) {
        opcode = DecodeType(pos, endpos);
        if (opcode == Instruction::RETURN) {
            default_asn = DecodeASN(pos, endpos);
            if (default_asn == INVALID) break;
            return default_asn;
        } else if (opcode == Instruction::JUMP) {
            jump = DecodeJump(pos, endpos);
            if (jump == INVALID) break;
            if (bits == 0) break;
            if (static_cast<int64_t>(jump) >= static_cast<int64_t>(endpos - pos)) break;
            if (ip[ip.size() - bits]) {
                pos += jump;
            }
            bits--;
        } else if (opcode == Instruction::MATCH) {
            match = DecodeMatch(pos, endpos);
            if (match == INVALID) break;
            matchlen = CountBits(match) - 1;
            if (bits < matchlen) break;
            for (uint32_t bit = 0; bit < matchlen; bit++) {
                if ((ip[ip.size() - bits]) != ((match >> (matchlen - 1 - bit)) & 1)) {
                    return default_asn;
                }
                bits--;
            }
        } else if (opcode == Instruction::DEFAULT) {
            default_asn = DecodeASN(pos, endpos);
            if (default_asn == INVALID) break;
        } else {
            break;
        }
    }
    assert(false);
    return 0;
}

bool SanityCheckASMap(const std::vector<bool>& asmap, int bits)
{
    const std::vector<bool>::const_iterator begin = asmap.begin(), endpos = asmap.end();
    std::vector<bool>::const_iterator pos = begin;
    std::vector<std::pair<uint32_t, int> > jumps;
    jumps.reserve(bits);
    Instruction prevopcode = Instruction::JUMP;
    bool had_incomplete_match = false;
    while (pos != endpos) {
        uint32_t offset = pos - begin;
        if (!jumps.empty() && offset >= jumps.back().first) return false;
        Instruction opcode = DecodeType(pos, endpos);
        if (opcode == Instruction::RETURN) {
            if (prevopcode == Instruction::DEFAULT) return false;
            uint32_t asn = DecodeASN(pos, endpos);
            if (asn == INVALID) return false;
            if (jumps.empty()) {
                if (endpos - pos > 7) return false;
                while (pos != endpos) {
                    if (*pos) return false;
                    ++pos;
                }
                return true;
            } else {
                offset = pos - begin;
                if (offset != jumps.back().first) return false;
                bits = jumps.back().second;
                jumps.pop_back();
                prevopcode = Instruction::JUMP;
            }
        } else if (opcode == Instruction::JUMP) {
            uint32_t jump = DecodeJump(pos, endpos);
            if (jump == INVALID) return false;
            if (static_cast<int64_t>(jump) > static_cast<int64_t>(endpos - pos)) return false;
            if (bits == 0) return false;
            --bits;
            uint32_t jump_offset = pos - begin + jump;
            if (!jumps.empty() && jump_offset >= jumps.back().first) return false;
            jumps.push_back(std::make_pair(jump_offset, bits));
            prevopcode = Instruction::JUMP;
        } else if (opcode == Instruction::MATCH) {
            uint32_t match = DecodeMatch(pos, endpos);
            if (match == INVALID) return false;
            int matchlen = CountBits(match) - 1;
            if (prevopcode != Instruction::MATCH) had_incomplete_match = false;
            if (matchlen < 8 && had_incomplete_match) return false;
            had_incomplete_match = (matchlen < 8);
            if (bits < matchlen) return false;
            bits -= matchlen;
            prevopcode = Instruction::MATCH;
        } else if (opcode == Instruction::DEFAULT) {
            if (prevopcode == Instruction::DEFAULT) return false;
            uint32_t asn = DecodeASN(pos, endpos);
            if (asn == INVALID) return false;
            prevopcode = Instruction::DEFAULT;
        } else {
            return false;
        }
    }
    return false;
}

std::vector<bool> DecodeAsmap(const std::string& path)
{
    std::vector<bool> bits;
    FILE* filestr = fopen(path.c_str(), "rb");
    if (!filestr) {
        LogPrintf("Failed to open asmap file %s\n", path);
        return bits;
    }
    if (fseek(filestr, 0, SEEK_END) != 0) {
        fclose(filestr);
        return bits;
    }
    long length = ftell(filestr);
    if (length < 0) {
        fclose(filestr);
        return bits;
    }
    LogPrintf("Opened asmap file %s (%ld bytes) from disk\n", path, length);
    if (fseek(filestr, 0, SEEK_SET) != 0) {
        fclose(filestr);
        return bits;
    }
    for (long i = 0; i < length; ++i) {
        int c = fgetc(filestr);
        if (c == EOF) {
            fclose(filestr);
            LogPrintf("Short read on asmap file %s\n", path);
            return std::vector<bool>();
        }
        uint8_t cur_byte = static_cast<uint8_t>(c);
        for (int bit = 0; bit < 8; ++bit) {
            bits.push_back((cur_byte >> bit) & 1);
        }
    }
    fclose(filestr);
    if (!SanityCheckASMap(bits, 128)) {
        LogPrintf("Sanity check of asmap file %s failed\n", path);
        return std::vector<bool>();
    }
    return bits;
}

void SetAsmap(std::vector<bool> asmap)
{
    g_asmap = std::move(asmap);
}

bool IsAsmapEnabled()
{
    return !g_asmap.empty();
}

size_t GetAsmapSize()
{
    return g_asmap.size();
}

uint32_t GetMappedAS(const CNetAddr& address)
{
    if (g_asmap.empty())
        return 0;
    // Only IPv4/IPv6; Tor and unroutable have no ASN mapping.
    if (address.IsTor() || address.IsLocal() || !address.IsRoutable())
        return 0;
    if (!address.IsIPv4() && !address.IsIPv6())
        return 0;

    std::vector<bool> ip_bits(128);
    if (address.IsIPv4()) {
        // Treat as IPv4-mapped IPv6: 80 zero bits, 16 ones, then 32 IPv4 bits
        for (int i = 0; i < 80; ++i)
            ip_bits[i] = false;
        for (int i = 80; i < 96; ++i)
            ip_bits[i] = true;
        const uint32_t ipv4 = (static_cast<uint32_t>(address.GetByte(3)) << 24) |
                              (static_cast<uint32_t>(address.GetByte(2)) << 16) |
                              (static_cast<uint32_t>(address.GetByte(1)) << 8) |
                              static_cast<uint32_t>(address.GetByte(0));
        for (int i = 0; i < 32; ++i)
            ip_bits[96 + i] = (ipv4 >> (31 - i)) & 1;
    } else {
        // Full IPv6: GetByte(15-n) is network-order byte n
        for (int byte_i = 0; byte_i < 16; ++byte_i) {
            const uint8_t cur_byte = static_cast<uint8_t>(address.GetByte(15 - byte_i));
            for (int bit_i = 0; bit_i < 8; ++bit_i)
                ip_bits[byte_i * 8 + bit_i] = (cur_byte >> (7 - bit_i)) & 1;
        }
    }
    return InterpretASMap(g_asmap, ip_bits);
}
