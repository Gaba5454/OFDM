#pragma once

#include "const.h"
#include "modulation_map.h"

#include <vector>

enum class ChannelEstimatorMethod {
    TrainingLS,
    PilotLSLinear,
    PilotLMMSE,
    DFTLS,
    DecisionDirected
};

const std::vector<ChannelEstimatorMethod>& channel_estimator_methods();
const char* channel_estimator_name(ChannelEstimatorMethod method);

std::vector<CF> estimate_live_channel(
    const std::vector<CF>& payload_freq,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method);

std::vector<CF> equalize_live_symbol(
    const std::vector<CF>& payload_freq,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method,
    std::vector<CF>* channel_estimate = nullptr);

DecodedResult decode_ofdm_stream_with_training(
    const std::vector<CF>& training_symbol,
    const std::vector<CF>& data_fixed,
    const ModulationSpec& modulation,
    size_t n_fft,
    size_t n_cp,
    size_t used_data_subcarriers,
    ChannelEstimatorMethod estimator_method = ChannelEstimatorMethod::TrainingLS,
    int spectrum_mode = 0,
    int data_order_mode = 0,
    int subcarrier_shift = 0);

double estimate_training_channel_roughness(
    const std::vector<CF>& training_symbol,
    size_t n_fft,
    size_t n_cp,
    int spectrum_mode = 0,
    int subcarrier_shift = 0);

double estimate_training_match_score(
    const std::vector<CF>& training_symbol,
    size_t n_fft,
    size_t n_cp,
    int spectrum_mode = 0,
    int subcarrier_shift = 0);

double estimate_training_decode_score(
    const std::vector<CF>& training_symbol,
    size_t n_fft,
    size_t n_cp,
    size_t used_data_subcarriers,
    int spectrum_mode = 0,
    int data_order_mode = 0,
    int subcarrier_shift = 0);
