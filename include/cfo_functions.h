#pragma once

#include "const.h"

double estimate_cfo(
    const std::vector<CF>& symbol_with_cp, 
    size_t n_fft, 
    size_t n_cp,
    double min_magnitude = 1e-3);
std::vector<CF> compensate_cfo(
    const std::vector<CF>& rx_data, 
    double cfo_normalized,
    size_t n_fft, 
    size_t n_cp);