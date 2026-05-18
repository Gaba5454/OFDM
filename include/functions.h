#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include "const.h" 

std::vector<int8_t> string_to_bits(const std::string& text);
std::vector<CF> cyclicPrefix(const std::vector<CF>& symbol, size_t cp_len);
std::vector<CF> PSS(size_t NID);
std::vector<CF> channelSimulation(const std::vector<CF>& arrayForTx, size_t arr_len, double noise_stddev = 1.0);
std::vector<double> correlationPSS(const std::vector<CF>& RxArray, const std::vector<CF>& PSS);
std::vector<size_t> findPeaks(const std::vector<double>& corrArr, double threshold);
std::vector<CF> extractDataAfterPSS(size_t peak_pos, std::vector<CF> array_for_tx);
double estimate_cfo(const std::vector<CF>& symbol_with_cp, size_t n_fft, size_t n_cp);
std::vector<CF> compensate_cfo(const std::vector<CF>& rx_data, double cfo_normalized, size_t n_fft, size_t n_cp);
DecodedResult decode_ofdm_stream(const std::vector<CF>& data_fixed, size_t n_fft, size_t n_cp);
std::string bits_to_string(const std::vector<int8_t>& bits); 
#endif