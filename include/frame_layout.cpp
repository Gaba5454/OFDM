#include "frame_layout.h"

#include <algorithm>
#include <array>

namespace {

std::vector<size_t> build_payload_data_indices()
{
    std::array<bool, LTE> is_pilot{};
    for (size_t idx : pilot_subcarrier_indices()) {
        is_pilot[idx] = true;
    }

    std::vector<size_t> data_indices;
    for (size_t i = 28; i <= 63; ++i) {
        if (!is_pilot[i]) {
            data_indices.push_back(i);
        }
    }
    for (size_t i = 65; i <= 100; ++i) {
        if (!is_pilot[i]) {
            data_indices.push_back(i);
            
        }
    }

    return data_indices;
}

}  // namespace

const std::vector<size_t>& pilot_subcarrier_indices()
{
    static const std::vector<size_t> indices = {28, 38, 48, 58, 68, 78, 88, 98};
    return indices;
}

const std::vector<size_t>& payload_data_indices()
{
    static const std::vector<size_t> indices = build_payload_data_indices();
    return indices;
}

size_t payload_bytes_per_symbol(const ModulationSpec& modulation)
{
    return payload_data_indices().size() * modulation.bits_per_symbol / 8;
}

size_t payload_symbol_count_for_text(size_t text_len, const ModulationSpec& modulation)
{
    const size_t bytes_per_symbol = payload_bytes_per_symbol(modulation);
    if (bytes_per_symbol == 0) {
        return 0;
    }

    const size_t clamped_text_len = std::min(text_len, MAX_TEXT_BYTES);
    const size_t total_payload_bytes = clamped_text_len + 5;
    return std::max<size_t>(1, (total_payload_bytes + bytes_per_symbol - 1) / bytes_per_symbol);
}

std::vector<CF> make_training_symbols(size_t symbol_count)
{
    std::vector<CF> symbols;
    symbols.reserve(symbol_count);
    uint32_t state = 0x5A17u;

    for (size_t i = 0; i < symbol_count; ++i) {
        state = state * 1664525u + 1013904223u;
        const float re = ((state >> 31) & 1u) ? 1.0f : -1.0f;
        state = state * 1664525u + 1013904223u;
        const float im = ((state >> 31) & 1u) ? 1.0f : -1.0f;
        symbols.emplace_back(re, im);
    }

    return symbols;
}
