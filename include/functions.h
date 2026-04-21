#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include "const.h" 

std::vector<int8_t> string_to_bits(const std::string& text);
std::vector<ComplexSymbol> cyclicPrefix(const std::vector<ComplexSymbol>& symbol, size_t cp_len);

#endif