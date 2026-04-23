#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include "const.h" 

std::vector<int8_t> string_to_bits(const std::string& text);
std::vector<CD> cyclicPrefix(const std::vector<CD>& symbol, size_t cp_len);
std::vector<CD> PSS(size_t NID);

#endif