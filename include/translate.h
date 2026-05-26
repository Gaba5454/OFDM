#pragma once

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include "const.h"
#include "another_functions.h"

void ModeTX(SoapySDRDevice *sdr, std::vector<CF>& tx_array, size_t iteration_count);
std::vector<CF> buildTxFrame(size_t num_iterations, const std::vector<CF>& pss_symbol, const std::vector<CF>& data_symbol, size_t pss_period); 