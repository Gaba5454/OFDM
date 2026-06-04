#include "cfo_functions.h"

double estimate_cfo(
    const std::vector<CF>& symbol_with_cp, 
    size_t n_fft, 
    size_t n_cp,
    double min_magnitude)
{

    if (symbol_with_cp.size() < n_fft + n_cp || n_cp == 0 || n_fft == 0) {
        return NAN; 
    }

    CF sum(0.0, 0.0);

    for (size_t i = 0; i < n_cp; ++i) {
        const CF& val_cp   = symbol_with_cp[i];
        const CF& val_tail = symbol_with_cp[n_fft + i];
        sum += val_cp * std::conj(val_tail);
    }

    if (std::abs(sum) < min_magnitude) {
        return NAN;  
    }

    double avg_phase = std::arg(sum);
    return avg_phase / (2.0 * M_PI * static_cast<double>(n_fft));
}


std::vector<CF> compensate_cfo(
    const std::vector<CF>& rx_data, 
    double cfo_normalized, 
    size_t n_fft, 
    size_t n_cp)
{
    if (rx_data.empty() || cfo_normalized == 0.0) {
        return rx_data;
    }
    if (std::isnan(cfo_normalized) || std::isinf(cfo_normalized)) {
        return rx_data;
    }

    double cfo_hz = cfo_normalized * 1e6; 

    return add_CFO(rx_data, -cfo_hz, 1e6);
}

std::vector<CF> add_CFO(const std::vector<CF>& signal, double CFO_hz, double F_srate) {
    double phase = 0.0;
    std::vector<CF> signal_CFO(signal.size());
    
    double phase_increment = 2.0 * M_PI * CFO_hz / F_srate;

    for (size_t i = 0; i < signal.size(); ++i) {

        signal_CFO[i] = signal[i] * std::exp(CF(0.0, phase));
        phase += phase_increment;
    }
    return signal_CFO;
}