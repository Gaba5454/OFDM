#pragma once

#include "const.h"

std::vector<CF> estimate_channel_with_pilots_linear(
    const std::vector<CF>& freq_symbol,
    const std::vector<size_t>& pilot_indices,
    const CF& pilot_symbol);

std::vector<CF> equalize_with_channel(
    const std::vector<CF>& freq_symbol,
    const std::vector<CF>& channel_estimate);

std::vector<CF> equalize_with_pilots(
    const std::vector<CF>& freq_symbol,
    const std::vector<size_t>& pilot_indices,
    const CF& pilot_symbol);
