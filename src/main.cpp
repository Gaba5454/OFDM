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
        const size_t CP_LENGTH = 20;  // Length of the CP ~1/12 on 128 symbols
        std::vector<CD> ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);

        // Make PSS and OFDM-symbol Tx
        size_t iter = 2;
        for(int i = 0; i < 2; i++){

        }

        // Visualization
        run_gui(text, raw_bits, symbols, pssSignal, ofdm_symbols, ofdm_with_cp);
        return 0;
}
