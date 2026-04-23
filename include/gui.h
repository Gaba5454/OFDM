#ifndef GUI_H
#define GUI_H

#include "const.h"
#include <vector>
#include <string>

void run_gui(
    const std::string& original_text,
    const std::vector<int8_t>& raw_bits,
    const std::vector<CD>& qpsk_symbols,
    const std::vector<CD>& pss_signal,
    const std::vector<CD>& ofdm_symbols,
    const std::vector<CD>& ofdm_with_cp
);

#endif 