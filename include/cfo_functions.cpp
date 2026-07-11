#include "cfo_functions.h"

#include <cmath>
#include <limits>

namespace {

std::vector<CF> add_cfo(const std::vector<CF>& signal, double cfo_hz, double sample_rate)
{
    double phase = 0.0;
    std::vector<CF> signal_with_cfo(signal.size());
    const double phase_increment = 2.0 * M_PI * cfo_hz / sample_rate;

    for (size_t i = 0; i < signal.size(); ++i) {
        signal_with_cfo[i] = signal[i] * std::exp(CF(0.0f, static_cast<float>(phase)));
        phase += phase_increment;
    }

    return signal_with_cfo;
}

}  // namespace

double estimate_cfo(
    const std::vector<CF>& symbol_with_cp,
    size_t n_fft,
    size_t n_cp,
    double min_magnitude)
{
    if (symbol_with_cp.size() < n_fft + n_cp || n_cp == 0 || n_fft == 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    CF sum(0.0f, 0.0f);
    for (size_t i = 0; i < n_cp; ++i) {
        const CF& val_cp = symbol_with_cp[i];
        const CF& val_tail = symbol_with_cp[n_fft + i];
        sum += val_cp * std::conj(val_tail);
    }

    if (std::abs(sum) < min_magnitude) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double avg_phase = std::arg(sum);
    return avg_phase / (2.0 * M_PI * static_cast<double>(n_fft));
}

std::vector<CF> compensate_cfo(
    const std::vector<CF>& rx_data,
    double cfo_normalized)
{
    if (rx_data.empty() || cfo_normalized == 0.0) {
        return rx_data;
    }
    if (std::isnan(cfo_normalized) || std::isinf(cfo_normalized)) {
        return rx_data;
    }

    const double cfo_hz = cfo_normalized * SDR_SAMPLE_RATE;
    return add_cfo(rx_data, cfo_hz, SDR_SAMPLE_RATE);
}
