#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include "const.h" 

std::vector<int8_t> string_to_bits(const std::string& text);
std::vector<CD> cyclicPrefix(const std::vector<CD>& symbol, size_t cp_len);
std::vector<CD> PSS(size_t NID);
std::vector<CD> correlation(std::vector<CD>& RxArray, std::vector<CD>& PSS); 
std::vector<size_t> findPeaks(const std::vector<double>& corrArr, double threshold);

#endif