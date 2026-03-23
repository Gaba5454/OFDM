// main_imgui.cpp
#include <iostream>
#include <fftw3.h>
#include <math.h>
#include <complex>
#include <vector>
#include <bitset>
#include <fstream>
#define LTE 128

using ComplexSymbol = std::complex<double>;

std::vector<int8_t> string_to_bits(const std::string& text) {
    std::vector<int8_t> bits;
    bits.reserve(text.size() * 8);
    
    for (unsigned char c : text) {  

        for (int i = 7; i >= 0; --i) {
            bits.push_back((c >> i) & 1);
        }
    }
    return bits;
}

std::vector<ComplexSymbol> QPSK(const std::vector<int8_t> &in_bits) {
    
    // Переменная количество выходных символов
    size_t num_symbols = in_bits.size() / 2;

    // Объявляется переменная для выходного массива, и сразу же удобно 
// под неё выделяется память на количество выходных символов
    std::vector<ComplexSymbol> out_sym;
    out_sym.reserve(num_symbols);



    // Цикл преобразования битов в символы, в конце каждой итерации
// Вычисляется финальная координата точки (с нормализацией).
// Конструируется объект std::complex прямо в памяти вектора.
// Увеличивается размер вектора на 1.
    for(size_t i = 0; i < in_bits.size(); i+=2) {
        int8_t bit0 = in_bits[i];
        int8_t bit1 = in_bits[i+1];

        double i_component = (bit0 == 0) ? -1.0f : 1.0f;
        double q_component = (bit1 == 0) ? -1.0f : 1.0f;

        out_sym.emplace_back(i_component, q_component);
    }
    return out_sym;
}

std::vector<ComplexSymbol> IFFT_LTE(const std::vector<ComplexSymbol>& in_sym) {
    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);

    for(size_t i = 0; i < LTE; ++i) {
        in[i][0] = 0.0;
        in[i][1] = 0.0;
    }

    size_t sym_idx = 0; 

    // Левая часть
    for (size_t i = 28; i <= 63 && sym_idx < in_sym.size(); ++i) {
        in[i][0] = in_sym[sym_idx].real();
        in[i][1] = in_sym[sym_idx].imag();
        ++sym_idx; 
    }

    // Правая часть
    for (size_t i = 65; i <= 100 && sym_idx < in_sym.size(); ++i) {
        in[i][0] = in_sym[sym_idx].real();
        in[i][1] = in_sym[sym_idx].imag();
        ++sym_idx;
    }
    fftw_plan plan = fftw_plan_dft_1d(
                                      LTE, 
                                      in, 
                                      out,
                                      FFTW_BACKWARD,
                                      FFTW_ESTIMATE
                                    );
    
    fftw_execute(plan);

    std::vector<ComplexSymbol> out_sig(LTE);
    for(size_t i = 0; i < LTE; ++i){
        out_sig[i] = ComplexSymbol(out[i][0],
                                   out[i][1]);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return out_sig;
}

std::vector<ComplexSymbol> cyclicPrefix(const std::vector<ComplexSymbol>& symbol, size_t cp_len) {

    if(cp_len >= symbol.size()){
        throw std::invalid_argument("Длина должна быть меньше размера символа.");
    }

    std::vector<ComplexSymbol> output;

    output.reserve(symbol.size()+cp_len);

    // 1. Копируем последние cp_len отсчётов в начало
    output.insert(output.end(), 
                  symbol.end() - cp_len, 
                  symbol.end());
    
    // 2. Копируем основной символ целиком
    output.insert(output.end(), 
                  symbol.begin(), 
                  symbol.end());

    return output; 
}

int main() {

        std::string text = "BUREAU1440thebest!";
        std::cout << "Исходный текст: \"" << text << "\"" << std::endl;
        std::cout << "Длина: " << text.size() << " символов = " << text.size() * 8 << " бит" << std::endl;
        
        std::vector<int8_t> raw_bits = string_to_bits(text);
        
        std::cout << "\nБиты: ";
        for (size_t i = 0; i < 144; i++) {
            std::cout << (int)raw_bits[i];
        }
        std::cout << std::endl;
        
        std::vector<ComplexSymbol> symbols = QPSK(raw_bits);
        int count = 0;
        std::cout << "\nСимволы: ";
        for (size_t i = 0; i < 72; i++) {
            std::cout << (ComplexSymbol)symbols[i];
            count++;
        }
        std::cout << std::endl << count;
        std::cout << std::endl;

        std::vector<ComplexSymbol> ofdm_symbols = IFFT_LTE(symbols);
        int count1 = 0;
        std::cout << "\nОБПФ: ";
        for (size_t i = 0; i < 128; i++) {
            std::cout << (ComplexSymbol)ofdm_symbols[i];
            count1++;
        }
        std::cout << std::endl << count1;
        std::cout << std::endl;

        // === Добавление циклического префикса ===
        const size_t CP_LENGTH = 10;  // ~1/12 от 128
        std::vector<ComplexSymbol> ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
        std::cout << "\n✓ Символ с циклическим префиксом: " << ofdm_with_cp.size() << " отсчётов" << std::endl;

       return 0;
}


    