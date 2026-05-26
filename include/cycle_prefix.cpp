#include "cycle_prefix.h"

/**
 * @brief Добавляет циклический префикс к OFDM-символу.
 * @param ofdm_symbol Вектор комплексных отсчётов символа.
 * @param cp_len Длина циклического префикса (в отсчётах).
 * @return Вектор [ CP | original_symbol ].
 * @throws std::invalid_argument если cp_len >= ofdm_symbol.size()
 */
std::vector<CF> cyclicPrefix(const std::vector<CF>& ofdm_symbol, size_t cp_len) {

    if(cp_len > ofdm_symbol.size()){
        throw std::invalid_argument("Длина должна быть меньше размера символа.");
    }

    if (cp_len == 0) {
        return ofdm_symbol;
    }

    std::vector<CF> output;

    output.reserve(ofdm_symbol.size()+cp_len);

    // Копируем последние cp_len отсчётов в начало
    output.insert(output.end(), 
                  ofdm_symbol.end() - cp_len, 
                  ofdm_symbol.end());
    
    // Копируем основной символ целиком
    output.insert(output.end(), 
                  ofdm_symbol.begin(), 
                  ofdm_symbol.end());

    return output; 
}