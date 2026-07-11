#pragma once

#include "const.h"
#include "modulation_map.h"
#include "training_decoder.h"

#include <SoapySDR/Device.h>

#include <string>
#include <vector>

// Synchronization result after scanning the RX buffer for PSS candidates.
struct SyncInfo {
    size_t pss_peak = SIZE_MAX;
    size_t pss_start = 0;
    size_t training_start = 0;
    size_t payload_start = 0;
    double pss_corr = 0.0;
    double cfo_hz = 0.0;
    size_t candidates = 0;
};

// Full decode result for one captured frame.
struct CaptureInfo {
    std::vector<double> corr_map;
    SyncInfo sync;
    DecodedResult decoded;
    std::vector<CF> data_after_pss;
    double coarse_cfo = 0.0;
    double fine_cfo = 0.0;
    double cfo = 0.0;
    int spectrum_mode = 0;
    int subcarrier_shift = 0;
    int data_order = 0;
    bool frame_found = false;
    bool payload_ready = false;
};

// Lightweight live analysis used by the real-time RX monitor.
struct LiveMonitorInfo {
    std::vector<CF> constellation_points;
    std::vector<CF> channel_estimate;
    std::vector<CF> equalized_symbol;
    size_t symbol_start = SIZE_MAX;
    double cp_score = 0.0;
    int spectrum_mode = 0;
    int subcarrier_shift = 0;
    bool available = false;
};

struct DecodeSearchHints {
    bool use_spectrum_mode = false;
    int spectrum_mode = 0;
    bool use_subcarrier_shift = false;
    int subcarrier_shift = 0;
};

// Device helpers.
SoapySDRDevice* open_pluto(const char* device_uri, std::string* error_message = nullptr);
std::vector<CF> slice_samples(const std::vector<CF>& samples, size_t start, size_t len);
double cp_metric(const std::vector<CF>& samples, size_t start);
std::vector<double> correlation_pss_cfo_search(
    const std::vector<CF>& rx_samples,
    const std::vector<CF>& pss,
    std::vector<double>& best_cfo_hz);

// Frame-level processing.
CaptureInfo decode_capture(
    const std::vector<CF>& rx_samples,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method = ChannelEstimatorMethod::TrainingLS,
    const DecodeSearchHints* search_hints = nullptr);
LiveMonitorInfo analyze_live_monitor(
    const std::vector<CF>& rx_samples,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method = ChannelEstimatorMethod::PilotLSLinear);
