// main.cpp
#include "../include/functions.h"
#include "../include/modulations.h"
#include "../include/ofdm.h"

int main() {

        std::string text = "BUREAU1440";
        std::cout << "Исходный текст: \"" << text << "\"" << std::endl;
        std::cout << "Длина: " << text.size() << " символов = " << text.size() * 8 << " бит" << std::endl;
        
        std::vector<int8_t> raw_bits = string_to_bits(text);
        
        std::cout << "\nБиты: ";
        for (size_t i = 0; i < text.size() * 8; i++) {
            std::cout << (int)raw_bits[i];
        }
        std::cout << std::endl;
        
        std::vector<ComplexSymbol> symbols = QPSK(raw_bits);
        int count = 0;
        std::cout << "\nСимволы: ";
        for (size_t i = 0; i < symbols.size(); i++) {
            std::cout << (ComplexSymbol)symbols[i];
            count++;
        }
        std::cout << std::endl << "Всего символов: " << count;
        std::cout << std::endl;

        std::vector<ComplexSymbol> ofdm_symbols = OFDM(symbols);
        int count1 = 0;
        std::cout << "\nОБПФ: ";
        for (size_t i = 0; i < ofdm_symbols.size(); i++) {
            std::cout << (ComplexSymbol)ofdm_symbols[i];
            count1++;
        }
        std::cout << std::endl << count1;
        std::cout << std::endl;

        // === Добавление циклического префикса ===
        const size_t CP_LENGTH = 20;  // ~1/12 от 128
        std::vector<ComplexSymbol> ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
        std::cout << "\n✓ Символ с циклическим префиксом: " << ofdm_with_cp.size() << " отсчётов" << std::endl;

       return 0;
}
