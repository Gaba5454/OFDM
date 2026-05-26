#pragma once

#include <iostream>
#include <complex>
#include <vector>
#include <cstdint>
#include <cstddef>  

constexpr std::complex<float> j(0.0, 1.0);

constexpr size_t LTE = 128;
constexpr size_t CP_LENGTH = 20;  // Length of the CP ~1/12 on 128 symbols
constexpr size_t SYMBOL_LEN = LTE + CP_LENGTH; // 148

using CF = std::complex<float>;
constexpr size_t iter = 10; 
struct DecodedResult {
    std::string recovered_text;
    std::vector<CF> constellation_points; // Для визуализации созвездия всех символов
    size_t symbols_processed;
};

constexpr double QPSK_LOW = -1.0;
constexpr double QPSK_HIGH = +1.0;
