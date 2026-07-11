#pragma once

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#include "const.h"

struct TxStreamContext {
    SoapySDRDevice* sdr = nullptr;
    SoapySDRStream* stream = nullptr;
    size_t mtu = 0;
};

bool open_tx_stream(SoapySDRDevice* sdr, TxStreamContext& ctx, bool verbose = true);
bool write_tx_frame(
    TxStreamContext& ctx,
    const std::vector<CF>& tx_array,
    size_t iteration_count,
    bool verbose = true);
void close_tx_stream(TxStreamContext& ctx);
