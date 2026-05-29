#pragma once

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "const.h"

std::vector<CF> ModeRX(SoapySDRDevice *sdr, size_t iteration_count);