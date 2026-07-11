#include "translate.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

std::vector<CF> scaled_tx_frame(const std::vector<CF>& tx_array)
{
    std::vector<CF> tx_scaled = tx_array;
    float peak = 0.0f;
    for (const CF& sample : tx_scaled) {
        peak = std::max(peak, std::abs(sample.real()));
        peak = std::max(peak, std::abs(sample.imag()));
    }

    const float scale = (peak > 0.95f) ? (0.95f / peak) : 1.0f;
    if (scale != 1.0f) {
        for (CF& sample : tx_scaled) {
            sample *= scale;
        }
    }

    return tx_scaled;
}

}  // namespace

bool open_tx_stream(SoapySDRDevice* sdr, TxStreamContext& ctx, bool verbose)
{
    ctx = {};
    ctx.sdr = sdr;

    constexpr double carrier_freq = 800e6;

    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, SDR_SAMPLE_RATE);
    SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_TX, 0, SDR_SAMPLE_RATE);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq, nullptr);
    SoapySDRDevice_setAntenna(sdr, SOAPY_SDR_TX, 0, "A");
    const double actual = SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_TX, 0);
    if (verbose) {
        std::cerr << "[MODE_TX] Freq set: " << (carrier_freq / 1e6)
                  << " MHz, Actual: " << (actual / 1e6) << " MHz\n";
    }

    size_t channels[] = {0};
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -20.0);
    const size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    SoapySDRKwargs stream_args = {};
    SoapySDRKwargs_set(&stream_args, "bufflen", "4096");
    ctx.stream = SoapySDRDevice_setupStream(
        sdr,
        SOAPY_SDR_TX,
        SOAPY_SDR_CF32,
        channels,
        channel_count,
        &stream_args
    );
    SoapySDRKwargs_clear(&stream_args);
    if (ctx.stream == nullptr) {
        std::cerr << "TX stream setup failed\n";
        return false;
    }

    SoapySDRDevice_activateStream(sdr, ctx.stream, 0, 0, 0);

    ctx.mtu = SoapySDRDevice_getStreamMTU(sdr, ctx.stream);
    double tx_full_scale = 32768.0;
    char* tx_native_format = SoapySDRDevice_getNativeStreamFormat(
        sdr,
        SOAPY_SDR_TX,
        0,
        &tx_full_scale
    );
    if (verbose && tx_native_format != nullptr) {
        std::cout << "TX native format: " << tx_native_format
                  << " full_scale=" << tx_full_scale << "\n";
        SoapySDR_free(tx_native_format);
    } else if (tx_native_format != nullptr) {
        SoapySDR_free(tx_native_format);
    }

    if (verbose) {
        std::cout << "tx_mtu = " << ctx.mtu << "\n";
    }

    if (ctx.mtu == 0) {
        close_tx_stream(ctx);
        return false;
    }

    return true;
}

bool write_tx_frame(
    TxStreamContext& ctx,
    const std::vector<CF>& tx_array,
    size_t iteration_count,
    bool verbose)
{
    if (ctx.stream == nullptr || ctx.mtu == 0 || tx_array.empty()) {
        return false;
    }

    const std::vector<CF> tx_scaled = scaled_tx_frame(tx_array);

    const long timeoutUs = 400000;

    std::vector<CF> tx_chunk(ctx.mtu);
    size_t total_samples_sent = 0;
    bool tx_failed = false;
    const bool run_forever = (iteration_count == 0);
    size_t frame_iter = 0;

    while (!tx_failed && (run_forever || frame_iter < iteration_count)) {
        size_t offset_samples = 0;
        while (offset_samples < tx_array.size()) {
            int flags = 0;
            const size_t remaining = tx_array.size() - offset_samples;
            const size_t chunk_samples = std::min(ctx.mtu, remaining);
            std::copy_n(
                tx_scaled.begin() + static_cast<ptrdiff_t>(offset_samples),
                static_cast<ptrdiff_t>(chunk_samples),
                tx_chunk.begin()
            );
            const void* tx_buffs[] = {tx_chunk.data()};

            const int st = SoapySDRDevice_writeStream(
                ctx.sdr,
                ctx.stream,
                tx_buffs,
                chunk_samples,
                &flags,
                0,
                timeoutUs
            );
            if (st < 0) {
                std::cerr << "TX error: " << st << "\n";
                tx_failed = true;
                break;
            }
            if (st == 0) {
                continue;
            }

            total_samples_sent += static_cast<size_t>(st);
            offset_samples += static_cast<size_t>(st);
        }
        ++frame_iter;
    }

    if (verbose) {
        std::cout << "TX: sent " << total_samples_sent
                  << " samples (~" << (static_cast<double>(total_samples_sent) / SDR_SAMPLE_RATE)
                  << " s)\n";
    }

    return !tx_failed;
}

void close_tx_stream(TxStreamContext& ctx)
{
    if (ctx.stream == nullptr || ctx.sdr == nullptr) {
        return;
    }

    SoapySDRDevice_deactivateStream(ctx.sdr, ctx.stream, 0, 0);
    SoapySDRDevice_closeStream(ctx.sdr, ctx.stream);
    ctx.stream = nullptr;
    ctx.mtu = 0;
}
