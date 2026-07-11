#pragma once

#include "const.h"

#include <cstdint>
#include <string>
#include <vector>

enum class ModulationType {
    BPSK,
    QPSK,
    QAM16,
    QAM64
};

struct ConstellationEntry {
    uint8_t bit_pattern = 0;
    CF symbol = CF(0.0f, 0.0f);
};

struct ModulationSpec {
    ModulationType type = ModulationType::QPSK;
    std::string name;
    size_t bits_per_symbol = 0;
    std::vector<ConstellationEntry> constellation;
};

const std::vector<ModulationSpec>& modulation_specs();
const ModulationSpec* find_modulation_spec(const std::string& name);

std::vector<CF> modulate_bits(const std::vector<uint8_t>& bits, const ModulationSpec& spec);
std::vector<int8_t> demodulate_symbol(const CF& symbol, const ModulationSpec& spec);
CF hard_decision_symbol(const CF& symbol, const ModulationSpec& spec);
std::vector<CF> ideal_constellation_points(const ModulationSpec& spec);
double constellation_mse(const std::vector<CF>& points, const ModulationSpec& spec);
