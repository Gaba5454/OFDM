#include "equalizer.h"

#include <algorithm>

namespace {

CF interpolate_channel(
    size_t bin_index,
    const std::vector<size_t>& pilot_indices,
    const std::vector<CF>& pilot_channel)
{
    if (pilot_indices.empty()) {
        return CF(1.0f, 0.0f);
    }

    if (bin_index <= pilot_indices.front()) {
        return pilot_channel.front();
    }
    if (bin_index >= pilot_indices.back()) {
        return pilot_channel.back();
    }

    auto upper_it = std::upper_bound(pilot_indices.begin(), pilot_indices.end(), bin_index);
    const size_t right_pos = static_cast<size_t>(std::distance(pilot_indices.begin(), upper_it));
    const size_t left_pos = right_pos - 1;

    const size_t left_bin = pilot_indices[left_pos];
    const size_t right_bin = pilot_indices[right_pos];
    if (right_bin == left_bin) {
        return pilot_channel[left_pos];
    }

    const float t = static_cast<float>(bin_index - left_bin) /
                    static_cast<float>(right_bin - left_bin);
    return pilot_channel[left_pos] + (pilot_channel[right_pos] - pilot_channel[left_pos]) * t;
}

}  // namespace

std::vector<CF> equalize_with_pilots(
    const std::vector<CF>& freq_symbol,
    const std::vector<size_t>& pilot_indices,
    const CF& pilot_symbol)
{
    std::vector<CF> equalized = freq_symbol;
    if (freq_symbol.empty() || pilot_indices.empty() || std::abs(pilot_symbol) < 1e-6f) {
        return equalized;
    }

    std::vector<CF> pilot_channel;
    pilot_channel.reserve(pilot_indices.size());

    for (size_t pilot_idx : pilot_indices) {
        if (pilot_idx >= freq_symbol.size()) {
            return equalized;
        }

        CF channel = freq_symbol[pilot_idx] / pilot_symbol;
        if (std::abs(channel) < 1e-6f) {
            channel = CF(1.0f, 0.0f);
        }
        pilot_channel.push_back(channel);
    }

    for (size_t idx = 0; idx < equalized.size(); ++idx) {
        if (idx < 28 || idx > 100 || idx == 64) {
            continue;
        }

        const CF channel = interpolate_channel(idx, pilot_indices, pilot_channel);
        if (std::abs(channel) < 1e-6f) {
            continue;
        }

        equalized[idx] /= channel;
    }

    return equalized;
}
