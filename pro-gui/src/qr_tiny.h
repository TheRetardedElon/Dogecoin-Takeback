#pragma once
#include <string>
#include <vector>
#include <cstdint>

/** Byte-mode QR (versions 1–10, ECC M). Empty on failure. */
std::vector<std::vector<uint8_t>> QrEncode(const std::string& text);
