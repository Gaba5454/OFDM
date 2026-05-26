#include "const.h"

std::vector<CF> bpsk(const std::vector<int8_t> &in_bits) {
    
    size_t num_symbols = in_bits.size();

    std::vector<CF> out_sym;
    out_sym.reserve(num_symbols);

    for(size_t i = 0; i < in_bits.size(); i++) {
        int8_t bit = in_bits[i];

        double i_component = (bit == 0) ? -1.0f : 1.0f;
        double q_component = 0;

        out_sym.emplace_back(i_component, q_component);
    }
    return out_sym;
}

std::vector<CF> qpsk(const std::vector<uint8_t>& in_bits) {
    
    if (in_bits.size() < 2) {
        return {};
    }

    auto bit_to_amp = [](uint8_t b) -> float {
        return (b == 0) ? QPSK_LOW : QPSK_HIGH;
    };

    const size_t num_symbols = in_bits.size() / 2;
    std::vector<CF> out_sym;
    out_sym.reserve(num_symbols);

    for (size_t s = 0; s < num_symbols; ++s) {
        float i_sym = bit_to_amp(in_bits[2*s]);
        float q_sym = bit_to_amp(in_bits[2*s + 1]);
        out_sym.emplace_back(i_sym, q_sym);
    }
    
    return out_sym;
}

std::vector<CF> QAM(const std::vector<int8_t> &in_bits) {
    
// Переменная количество выходных символов
    size_t num_symbols = in_bits.size() / 2;

// Объявляется переменная для выходного массива, и сразу же удобно 
// под неё выделяется память на количество выходных символов
    std::vector<CF> out_sym;
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