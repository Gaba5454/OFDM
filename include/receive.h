#pragma once

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "const.h"

void ModeRX(SoapySDRDevice *sdr, std::vector<CF>& tx_array, size_t iteration_count);