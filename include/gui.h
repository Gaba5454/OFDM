#pragma once

#include "const.h"

#include <string>
#include <vector>

// Offline snapshot for simulation/RX visualization.
struct GuiPlotData {
    std::string original_text;
    std::string modulation_name;
    std::vector<uint8_t> raw_bits;
    std::vector<CF> modulated_symbols;
    std::vector<CF> ideal_constellation;
    std::vector<CF> pss_signal;
    std::vector<CF> ofdm_symbols;
    std::vector<CF> ofdm_with_cp;
    std::vector<CF> tx_array;
    double snr = 0.0;
    std::vector<double> correlation_map;
    size_t peak_position = SIZE_MAX;
    std::vector<CF> data_after_pss;
    std::string recovered_text;
    std::vector<CF> received_constellation;
    bool crc_ok = false;
    size_t max_text_bytes = 0;
    bool text_was_truncated = false;
};

// Simulation windows.
void run_gui(const GuiPlotData& data);
int run_simulation_gui(
    const std::string& initial_text,
    double initial_snr,
    const std::string& initial_modulation_name);

// Real-time windows.
int run_realtime_tx_gui(
    const char* device_uri,
    const std::string& initial_text,
    const std::string& initial_modulation_name);
int run_realtime_rx_gui(
    const char* device_uri,
    const std::string& initial_modulation_name);
