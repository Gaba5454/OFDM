#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include "const.h" 

std::vector<int8_t> string_to_bits(const std::string& text);
std::vector<CD> cyclicPrefix(const std::vector<CD>& symbol, size_t cp_len);
std::vector<CD> PSS(size_t NID);
std::vector<double> correlationPSS(const std::vector<CD>& RxArray, const std::vector<CD>& PSS);
std::vector<size_t> findPeaks(const std::vector<double>& corrArr, double threshold);
std::vector<CD> extractDataAfterPSS(size_t peak_pos, std::vector<CD> array_for_tx);
double estimate_cfo(const std::vector<CD>& symbol_with_cp, size_t n_fft, size_t n_cp);
std::vector<CD> compensate_cfo(const std::vector<CD>& rx_data, double cfo_normalized);
DecodedResult decode_ofdm_stream(const std::vector<CD>& data_fixed, size_t n_fft, size_t n_cp);
std::string bits_to_string(const std::vector<int8_t>& bits); 
#endif