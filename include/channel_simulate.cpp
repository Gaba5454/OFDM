#include "channel_simulate.h"

#include <cmath>
#include <random>

namespace {

double calculate_signal_power(const std::vector<CF>& signal)
{
    if (signal.empty()) {
        return 0.0;
    }

    double power_sum = 0.0;
    for (const auto& s : signal) {
        power_sum += std::norm(s);
    }
    return power_sum / static_cast<double>(signal.size());
}

}  // namespace

std::vector<CF> channelSimulation(const std::vector<CF>& signal, double snr_dB)
{
    if (signal.empty()) {
        return {};
    }

    const double signal_power = calculate_signal_power(signal);
    const double snr_linear = std::pow(10.0, snr_dB / 10.0);
    const double noise_power = signal_power / snr_linear;
    const double noise_stddev = std::sqrt(noise_power / 2.0);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, noise_stddev);

    std::vector<CF> result;
    result.reserve(signal.size());

    for (const auto& sample : signal) {
        const double noise_i = dist(gen);
        const double noise_q = dist(gen);

        result.emplace_back(
            static_cast<float>(sample.real() + noise_i),
            static_cast<float>(sample.imag() + noise_q)
        );
    }

    return result;
}
