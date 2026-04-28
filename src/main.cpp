// main.cpp
#include "../include/functions.h"
#include "../include/modulations.h"
#include "../include/ofdm.h"
#include "../include/gui.h"
#include <iostream>

int main() {

        // Enter here your text
        std::string text = "BUREAU1440";

        // Transform string into bits
        std::vector<int8_t> raw_bits = string_to_bits(text);

        // Mapping symbols
        std::vector<CD> symbols = QPSK(raw_bits);
        int count = 0;

        // Create PSS
        std::vector<CD> pssSignal = PSS(1);

        // Create OFDM-symbol
        std::vector<CD> ofdm_symbols = OFDM(symbols);


        // Add cycle prefix
        std::vector<CD> ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
        std::vector<CD> pss_with_cp = cyclicPrefix(pssSignal, CP_LENGTH);

        // Make PSS and OFDM-symbol Tx
        size_t iter = 10; 
        std::vector<CD> array_for_tx;
        array_for_tx.reserve(iter * LTE+CP_LENGTH);

        // Array creation cycle on TX
        for(int i = 0; i < iter; i++){
                if (i % 5 == 0){
                        array_for_tx.insert(array_for_tx.end(), pss_with_cp.begin(), pss_with_cp.end());
                }
                else {
                        array_for_tx.insert(array_for_tx.end(), ofdm_with_cp.begin(), ofdm_with_cp.end());
                }
        }

        // Correlation
        std::vector<CD> corr_arr(correlation(array_for_tx, pssSignal));
        size_t peak_pos = 0;
        if (!corr_arr.empty()) {
                peak_pos = std::distance(corr_arr.begin(), std::max_element(corr_arr.begin(), corr_arr.end()));
        }
        
        // Visualization
        run_gui(text, raw_bits, symbols, pssSignal, ofdm_symbols, ofdm_with_cp, array_for_tx, corr_arr, peak_pos);
        return 0;
}


