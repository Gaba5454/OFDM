#include "demodulations.h"

std::vector<int8_t> qpsk_demodulate_symbol(const CF& symbol) {
    return {
        static_cast<int8_t>(symbol.real() >= 0.0f ? 1 : 0),  // I-бит → первый (старший в паре)
        static_cast<int8_t>(symbol.imag() >= 0.0f ? 1 : 0)   // Q-бит → второй
    };
}