#include "corellations.h"

#include <algorithm>
#include <cmath>
#include <numeric>

std::vector<double> correlationPSS(
    const std::vector<CF>& RxArray,
    const std::vector<CF>& PSS)
{
    const size_t len_rx = RxArray.size();
    const size_t len_pss = PSS.size();

    if (len_pss == 0 || len_rx < len_pss) {
        return {};
    }

    double pss_energy = 0.0;
    for (const auto& s : PSS) {
        pss_energy += std::norm(s);
    }
    if (pss_energy == 0.0) {
        return {};
    }

    const size_t output_size = len_rx - len_pss + 1;
    std::vector<double> corrArr;
    corrArr.reserve(output_size);

    for (size_t k = 0; k < output_size; ++k) {
        CF sum(0.0, 0.0);
        double rx_energy = 0.0;

        for (size_t n = 0; n < len_pss; ++n) {
            const CF& rx = RxArray[k + n];
            sum += rx * std::conj(PSS[n]);
            rx_energy += std::norm(rx);
        }

        double correlation = 0.0;
        if (rx_energy > 0.0) {
            correlation = std::abs(sum) / std::sqrt(pss_energy * rx_energy);
        }

        corrArr.push_back(correlation);
    }

    return corrArr;
}

CorrelationStats analyzeCorrelationMap(const std::vector<double>& corr_map) {
    CorrelationStats stats{SIZE_MAX, 0.0, 0.0, 0.0, 0.0};
    if (corr_map.empty()) {
        return stats;
    }

    auto max_it = std::max_element(corr_map.begin(), corr_map.end());
    stats.peak_index = static_cast<size_t>(std::distance(corr_map.begin(), max_it));
    stats.max_value = *max_it;

    const double sum = std::accumulate(corr_map.begin(), corr_map.end(), 0.0);
    stats.mean_value = sum / static_cast<double>(corr_map.size());

    double sq_sum = 0.0;
    for (double value : corr_map) {
        const double diff = value - stats.mean_value;
        sq_sum += diff * diff;
    }
    stats.stddev_value = std::sqrt(sq_sum / static_cast<double>(corr_map.size()));

    const double denom = std::max(stats.mean_value, 1e-9);
    stats.peak_to_mean = stats.max_value / denom;
    return stats;
}

size_t findCorrelationPeak(const std::vector<double>& corr_map) {
    if (corr_map.empty()) {
        return SIZE_MAX;
    }

    const CorrelationStats stats = analyzeCorrelationMap(corr_map);
    const double dynamic_floor = stats.mean_value + 4.0 * stats.stddev_value;

    if (stats.max_value < 0.12) {
        return SIZE_MAX;
    }
    if (stats.max_value < dynamic_floor) {
        return SIZE_MAX;
    }
    if (stats.peak_to_mean < 2.5) {
        return SIZE_MAX;
    }

    return stats.peak_index;
}
