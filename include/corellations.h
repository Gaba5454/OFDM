#pragma once

#include "const.h"
#include <algorithm>

std::vector<double> correlationPSS(const std::vector<CF>& RxArray, const std::vector<CF>& PSS);
size_t findCorrelationPeak(const std::vector<double>& corr_map);