#pragma once

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#include "const.h"

struct RxStreamContext {
    SoapySDRDevice* sdr = nullptr;
    SoapySDRStream* stream = nullptr;
    size_t mtu = 0;
};

bool open_rx_stream(SoapySDRDevice* sdr, RxStreamContext& ctx, bool verbose = true);
std::vector<CF> read_rx_samples(
    RxStreamContext& ctx,
    size_t iteration_count,
    size_t warmup_reads = 0,
    bool verbose = true,
    long timeout_us = 400000);
void close_rx_stream(RxStreamContext& ctx);
