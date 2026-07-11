#pragma once

#include "const.h"

struct CorrelationStats {
    size_t peak_index;
    double max_value;
    double mean_value;
    double stddev_value;
    double peak_to_mean;
};

std::vector<double> correlationPSS(const std::vector<CF>& RxArray, const std::vector<CF>& PSS);
CorrelationStats analyzeCorrelationMap(const std::vector<double>& corr_map);
size_t findCorrelationPeak(const std::vector<double>& corr_map);
