#include "cycle_prefix.h"

std::vector<CF> cyclicPrefix(const std::vector<CF>& ofdm_symbol, size_t cp_len) {

    if(cp_len > ofdm_symbol.size()){
        throw std::invalid_argument("Длина должна быть меньше размера символа.");
    }

    if (cp_len == 0) {
        return ofdm_symbol;
    }

    std::vector<CF> output;

    output.reserve(ofdm_symbol.size()+cp_len);

    output.insert(output.end(), 
                  ofdm_symbol.end() - cp_len, 
                  ofdm_symbol.end());
    
    output.insert(output.end(), 
                  ofdm_symbol.begin(), 
                  ofdm_symbol.end());

    return output; 
}