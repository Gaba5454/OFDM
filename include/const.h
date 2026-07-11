#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr std::complex<float> j(0.0f, 1.0f);

constexpr size_t LTE = 128;
constexpr size_t CP_LENGTH = 20;
constexpr size_t SYMBOL_LEN = LTE + CP_LENGTH;
constexpr double SDR_SAMPLE_RATE = 1e6;
constexpr size_t MAX_TEXT_BYTES = 100;

using CF = std::complex<float>;
constexpr CF KNOWN_PILOT(0.707f, 0.707f);

struct DecodedResult {
    std::string recovered_text;
    std::vector<CF> constellation_points;
    std::vector<CF> channel_estimate;
    std::vector<CF> equalized_symbol;
    size_t symbols_processed = 0;
    std::vector<uint8_t> raw_bytes;
    size_t payload_length = 0;
    bool crc_ok = false;
};
