#pragma once

#include "const.h"

std::vector<CF> equalize_with_pilots(
    const std::vector<CF>& freq_symbol,
    const std::vector<size_t>& pilot_indices,
    const CF& pilot_symbol);
