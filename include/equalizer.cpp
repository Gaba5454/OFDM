#include "equalizer.h"

#include <algorithm>
#include <cmath>

namespace {

float interpolate_scalar(
    size_t bin_index,
    const std::vector<size_t>& pilot_indices,
    const std::vector<float>& values)
{
    if (pilot_indices.empty()) {
        return 0.0f;
    }

    if (bin_index <= pilot_indices.front()) {
        return values.front();
    }
    if (bin_index >= pilot_indices.back()) {
        return values.back();
    }

    auto upper_it = std::upper_bound(pilot_indices.begin(), pilot_indices.end(), bin_index);
    const size_t right_pos = static_cast<size_t>(std::distance(pilot_indices.begin(), upper_it));
    const size_t left_pos = right_pos - 1;

    const size_t left_bin = pilot_indices[left_pos];
    const size_t right_bin = pilot_indices[right_pos];
    if (right_bin == left_bin) {
        return values[left_pos];
    }

    const float t = static_cast<float>(bin_index - left_bin) /
                    static_cast<float>(right_bin - left_bin);
    return values[left_pos] + (values[right_pos] - values[left_pos]) * t;
}

std::vector<float> unwrap_phases(const std::vector<CF>& pilot_channel)
{
    std::vector<float> phases;
    phases.reserve(pilot_channel.size());

    float previous = 0.0f;
    float offset = 0.0f;
    for (size_t i = 0; i < pilot_channel.size(); ++i) {
        float phase = std::arg(pilot_channel[i]);
        if (i > 0) {
            const float delta = phase - previous;
            if (delta > static_cast<float>(M_PI)) {
                offset -= static_cast<float>(2.0 * M_PI);
            } else if (delta < static_cast<float>(-M_PI)) {
                offset += static_cast<float>(2.0 * M_PI);
            }
        }
        phases.push_back(phase + offset);
        previous = phase;
    }

    return phases;
}

std::pair<float, float> fit_phase_line(
    const std::vector<size_t>& pilot_indices,
    const std::vector<float>& pilot_phases)
{
    if (pilot_indices.empty() || pilot_phases.empty()) {
        return {0.0f, 0.0f};
    }
    if (pilot_indices.size() == 1 || pilot_phases.size() == 1) {
        return {0.0f, pilot_phases.front()};
    }

    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;
    const double n = static_cast<double>(std::min(pilot_indices.size(), pilot_phases.size()));

    for (size_t i = 0; i < pilot_indices.size() && i < pilot_phases.size(); ++i) {
        const double x = static_cast<double>(pilot_indices[i]);
        const double y = static_cast<double>(pilot_phases[i]);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }

    const double denom = n * sxx - sx * sx;
    if (std::abs(denom) < 1e-9) {
        return {0.0f, static_cast<float>(sy / std::max(n, 1.0))};
    }

    const double slope = (n * sxy - sx * sy) / denom;
    const double intercept = (sy - slope * sx) / n;
    return {static_cast<float>(slope), static_cast<float>(intercept)};
}

}  // namespace

std::vector<CF> estimate_channel_with_pilots_linear(
    const std::vector<CF>& freq_symbol,
    const std::vector<size_t>& pilot_indices,
    const CF& pilot_symbol)
{
    std::vector<CF> channel_estimate(freq_symbol.size(), CF(1.0f, 0.0f));
    if (freq_symbol.empty() || pilot_indices.empty() || std::abs(pilot_symbol) < 1e-6f) {
        return channel_estimate;
    }

    std::vector<CF> pilot_channel;
    pilot_channel.reserve(pilot_indices.size());

    for (size_t pilot_idx : pilot_indices) {
        if (pilot_idx >= freq_symbol.size()) {
            return channel_estimate;
        }

        CF channel = freq_symbol[pilot_idx] / pilot_symbol;
        if (std::abs(channel) < 1e-6f) {
            channel = CF(1.0f, 0.0f);
        }
        pilot_channel.push_back(channel);
    }

    std::vector<float> pilot_magnitude;
    pilot_magnitude.reserve(pilot_channel.size());
    for (const CF& channel : pilot_channel) {
        pilot_magnitude.push_back(std::max(std::abs(channel), 1e-6f));
    }
    const std::vector<float> pilot_phase = unwrap_phases(pilot_channel);
    const auto [phase_slope, phase_intercept] = fit_phase_line(pilot_indices, pilot_phase);

    std::vector<float> phase_residual;
    phase_residual.reserve(pilot_phase.size());
    for (size_t i = 0; i < pilot_phase.size() && i < pilot_indices.size(); ++i) {
        const float trend = phase_slope * static_cast<float>(pilot_indices[i]) + phase_intercept;
        phase_residual.push_back(pilot_phase[i] - trend);
    }

    for (size_t idx = 0; idx < channel_estimate.size(); ++idx) {
        if (idx < 28 || idx > 100 || idx == 64) {
            continue;
        }

        const float magnitude = interpolate_scalar(idx, pilot_indices, pilot_magnitude);
        const float residual = interpolate_scalar(idx, pilot_indices, phase_residual);
        const float phase =
            phase_slope * static_cast<float>(idx) + phase_intercept + residual;
        const CF channel = std::polar(magnitude, phase);
        if (std::abs(channel) < 1e-6f) {
            continue;
        }

        channel_estimate[idx] = channel;
    }

    for (size_t i = 0; i < pilot_indices.size() && i < pilot_channel.size(); ++i) {
        const size_t pilot_idx = pilot_indices[i];
        if (pilot_idx < channel_estimate.size()) {
            channel_estimate[pilot_idx] = pilot_channel[i];
        }
    }

    return channel_estimate;
}

std::vector<CF> equalize_with_channel(
    const std::vector<CF>& freq_symbol,
    const std::vector<CF>& channel_estimate)
{
    std::vector<CF> equalized = freq_symbol;
    if (freq_symbol.size() != channel_estimate.size()) {
        return equalized;
    }

    for (size_t idx = 0; idx < equalized.size(); ++idx) {
        if (std::abs(channel_estimate[idx]) < 1e-6f) {
            continue;
        }
        equalized[idx] /= channel_estimate[idx];
    }

    return equalized;
}

std::vector<CF> equalize_with_pilots(
    const std::vector<CF>& freq_symbol,
    const std::vector<size_t>& pilot_indices,
    const CF& pilot_symbol)
{
    const std::vector<CF> channel_estimate =
        estimate_channel_with_pilots_linear(freq_symbol, pilot_indices, pilot_symbol);
    std::vector<CF> equalized = equalize_with_channel(freq_symbol, channel_estimate);
    return equalized;
}
