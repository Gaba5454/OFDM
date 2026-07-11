#include "modulation_map.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

float gray2_level(uint8_t bits)
{
    switch (bits & 0x3u) {
        case 0x0u: return -3.0f;
        case 0x1u: return -1.0f;
        case 0x3u: return  1.0f;
        default:   return  3.0f;
    }
}

float gray3_level(uint8_t bits)
{
    switch (bits & 0x7u) {
        case 0x0u: return -7.0f;
        case 0x1u: return -5.0f;
        case 0x3u: return -3.0f;
        case 0x2u: return -1.0f;
        case 0x6u: return  1.0f;
        case 0x7u: return  3.0f;
        case 0x5u: return  5.0f;
        default:   return  7.0f;
    }
}

std::vector<ConstellationEntry> make_bpsk_constellation()
{
    return {
        {0x0u, CF(-1.0f, 0.0f)},
        {0x1u, CF( 1.0f, 0.0f)}
    };
}

std::vector<ConstellationEntry> make_qpsk_constellation()
{
    return {
        {0x0u, CF(-1.0f, -1.0f)},
        {0x1u, CF(-1.0f,  1.0f)},
        {0x2u, CF( 1.0f, -1.0f)},
        {0x3u, CF( 1.0f,  1.0f)}
    };
}

std::vector<ConstellationEntry> make_qam16_constellation()
{
    constexpr float norm = 1.0f / std::sqrt(10.0f);
    std::vector<ConstellationEntry> entries;
    entries.reserve(16);

    for (uint8_t i_bits = 0; i_bits < 4; ++i_bits) {
        for (uint8_t q_bits = 0; q_bits < 4; ++q_bits) {
            const uint8_t pattern = static_cast<uint8_t>((i_bits << 2) | q_bits);
            entries.push_back({
                pattern,
                CF(gray2_level(i_bits) * norm, gray2_level(q_bits) * norm)
            });
        }
    }

    return entries;
}

std::vector<ConstellationEntry> make_qam64_constellation()
{
    constexpr float norm = 1.0f / std::sqrt(42.0f);
    std::vector<ConstellationEntry> entries;
    entries.reserve(64);

    for (uint8_t i_bits = 0; i_bits < 8; ++i_bits) {
        for (uint8_t q_bits = 0; q_bits < 8; ++q_bits) {
            const uint8_t pattern = static_cast<uint8_t>((i_bits << 3) | q_bits);
            entries.push_back({
                pattern,
                CF(gray3_level(i_bits) * norm, gray3_level(q_bits) * norm)
            });
        }
    }

    return entries;
}

uint8_t pack_bits(const std::vector<uint8_t>& bits, size_t start, size_t count)
{
    uint8_t pattern = 0;
    for (size_t i = 0; i < count && start + i < bits.size(); ++i) {
        pattern = static_cast<uint8_t>((pattern << 1) | (bits[start + i] & 1u));
    }
    return pattern;
}

std::vector<int8_t> unpack_bits(uint8_t pattern, size_t count)
{
    std::vector<int8_t> bits(count, 0);
    for (size_t i = 0; i < count; ++i) {
        const size_t shift = count - 1 - i;
        bits[i] = static_cast<int8_t>((pattern >> shift) & 1u);
    }
    return bits;
}

const ConstellationEntry& nearest_entry(const CF& symbol, const ModulationSpec& spec)
{
    const ConstellationEntry* best = &spec.constellation.front();
    double best_dist = std::numeric_limits<double>::infinity();

    for (const ConstellationEntry& entry : spec.constellation) {
        const double dist = std::norm(symbol - entry.symbol);
        if (dist < best_dist) {
            best = &entry;
            best_dist = dist;
        }
    }

    return *best;
}

}  // namespace

const std::vector<ModulationSpec>& modulation_specs()
{
    static const std::vector<ModulationSpec> specs = {
        {ModulationType::BPSK, "BPSK", 1, make_bpsk_constellation()},
        {ModulationType::QPSK, "QPSK", 2, make_qpsk_constellation()},
        {ModulationType::QAM16, "QAM16", 4, make_qam16_constellation()},
        {ModulationType::QAM64, "QAM64", 6, make_qam64_constellation()}
    };

    return specs;
}

const ModulationSpec* find_modulation_spec(const std::string& name)
{
    const auto& specs = modulation_specs();
    const auto it = std::find_if(specs.begin(), specs.end(), [&](const ModulationSpec& spec) {
        return spec.name == name;
    });

    return (it == specs.end()) ? nullptr : &(*it);
}

std::vector<CF> modulate_bits(const std::vector<uint8_t>& bits, const ModulationSpec& spec)
{
    if (spec.bits_per_symbol == 0 || bits.size() < spec.bits_per_symbol) {
        return {};
    }

    std::vector<CF> symbols;
    symbols.reserve(bits.size() / spec.bits_per_symbol);

    for (size_t i = 0; i + spec.bits_per_symbol <= bits.size(); i += spec.bits_per_symbol) {
        const uint8_t pattern = pack_bits(bits, i, spec.bits_per_symbol);
        const auto it = std::find_if(
            spec.constellation.begin(),
            spec.constellation.end(),
            [&](const ConstellationEntry& entry) { return entry.bit_pattern == pattern; }
        );
        if (it != spec.constellation.end()) {
            symbols.push_back(it->symbol);
        }
    }

    return symbols;
}

std::vector<int8_t> demodulate_symbol(const CF& symbol, const ModulationSpec& spec)
{
    if (spec.constellation.empty()) {
        return {};
    }

    const ConstellationEntry& entry = nearest_entry(symbol, spec);
    return unpack_bits(entry.bit_pattern, spec.bits_per_symbol);
}

CF hard_decision_symbol(const CF& symbol, const ModulationSpec& spec)
{
    if (spec.constellation.empty()) {
        return CF(0.0f, 0.0f);
    }

    return nearest_entry(symbol, spec).symbol;
}

std::vector<CF> ideal_constellation_points(const ModulationSpec& spec)
{
    std::vector<CF> points;
    points.reserve(spec.constellation.size());

    for (const ConstellationEntry& entry : spec.constellation) {
        points.push_back(entry.symbol);
    }

    return points;
}

double constellation_mse(const std::vector<CF>& points, const ModulationSpec& spec)
{
    if (points.empty() || spec.constellation.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    double mse = 0.0;
    for (const CF& point : points) {
        mse += std::norm(point - nearest_entry(point, spec).symbol);
    }

    return mse / static_cast<double>(points.size());
}
