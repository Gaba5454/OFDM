#pragma once

#include <algorithm>
#include <fftw3.h>
#include "const.h" 
#include "demodulations.h"

void to_cs16(const std::vector<CF>& src, std::vector<int16_t>& dst);
std::vector<uint8_t> string_to_bits(const std::string& text);
std::vector<CF> extractDataAfterPSS(size_t peak_pos, const std::vector<CF>& rx_array, size_t data_len);
DecodedResult decode_ofdm_stream(const std::vector<CF>& data_fixed, size_t n_fft, size_t n_cp);
std::vector<CF> buildTxFrame(
    size_t num_iterations,
    const std::vector<CF>& pss_symbol,
    const std::vector<CF>& data_symbol,
    size_t pss_period);
void print_usage(const char* prog_name);