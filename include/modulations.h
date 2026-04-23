#ifndef MODULATIONS_H
#define MODULATIONS_H
#include "const.h"

std::vector<CD> QPSK(const std::vector<int8_t> &in_bits);
std::vector<CD> BPSK(const std::vector<int8_t> &in_bits); 
#endif