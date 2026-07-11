#pragma once

#include "const.h"
#include "modulation_map.h"

#include <vector>

struct TxFrameData {
    std::vector<uint8_t> bits;
    std::vector<CF> modulated_symbols;
    std::vector<CF> pss_with_cp;
    std::vector<CF> training_ofdm;
    std::vector<CF> training_with_cp;
    std::vector<CF> samples;
    size_t max_text_bytes = MAX_TEXT_BYTES;
    size_t payload_symbol_count = 0;
    bool text_was_truncated = false;
};

TxFrameData build_tx_frame(const std::string& text, const ModulationSpec& modulation);
