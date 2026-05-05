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
    const std::vector<CD>& ofdm_with_cp,
    const std::vector<CD>& tx_array,
    const std::vector<double>& correlation_map,
    size_t peak_position,
    const std::vector<CD>& data_after_pss,
    const std::string& recovered_text,             
    const std::vector<CD>& received_constellation  
);

#endif 