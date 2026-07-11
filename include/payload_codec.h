#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

std::vector<uint8_t> bits_to_bytes(const std::vector<int8_t>& bits);
std::vector<uint8_t> bytes_to_bits(const std::vector<uint8_t>& bytes);
uint16_t crc16_ccitt(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> pack_payload_bytes(const std::string& text, size_t payload_bytes_capacity);
