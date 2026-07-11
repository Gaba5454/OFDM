#pragma once

#include "const.h"
#include "modulation_map.h"

#include <vector>

const std::vector<size_t>& pilot_subcarrier_indices();
const std::vector<size_t>& payload_data_indices();
size_t payload_bytes_per_symbol(const ModulationSpec& modulation);
size_t payload_symbol_count_for_text(size_t text_len, const ModulationSpec& modulation);
std::vector<CF> make_training_symbols(size_t symbol_count);
