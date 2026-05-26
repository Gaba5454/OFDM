#pragma once

#include <fftw3.h>
#include "const.h" 

std::vector<CF> ofdm(const std::vector<CF>& in_sym);