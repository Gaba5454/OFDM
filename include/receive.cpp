#include "receive.h"

#include <iostream>

bool open_rx_stream(SoapySDRDevice* sdr, RxStreamContext& ctx, bool verbose)
{
    ctx = {};
    ctx.sdr = sdr;

    constexpr double carrier_freq = 800e6;
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, SDR_SAMPLE_RATE);
    SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_RX, 0, SDR_SAMPLE_RATE);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, nullptr);
    SoapySDRDevice_setAntenna(sdr, SOAPY_SDR_RX, 0, "A_BALANCED");
    SoapySDRDevice_setGainMode(sdr, SOAPY_SDR_RX, 0, false);
    const double actual = SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_RX, 0);
    if (verbose) {
        std::cerr << "[MODE_RX] Freq set: " << (carrier_freq / 1e6)
                  << " MHz, Actual: " << (actual / 1e6) << " MHz\n";
    }

    size_t channels[] = {0};
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 10.0);

    const size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    SoapySDRKwargs stream_args = {};
    SoapySDRKwargs_set(&stream_args, "bufflen", "4096");
    ctx.stream = SoapySDRDevice_setupStream(
        sdr,
        SOAPY_SDR_RX,
        SOAPY_SDR_CF32,
        channels,
        channel_count,
        &stream_args
    );
    SoapySDRKwargs_clear(&stream_args);
    if (ctx.stream == nullptr) {
        std::cerr << "RX stream setup failed\n";
        return false;
    }

    SoapySDRDevice_activateStream(sdr, ctx.stream, 0, 0, 0);

    ctx.mtu = SoapySDRDevice_getStreamMTU(sdr, ctx.stream);
    double rx_full_scale = 2048.0;
    char* rx_native_format = SoapySDRDevice_getNativeStreamFormat(sdr, SOAPY_SDR_RX, 0, &rx_full_scale);
    if (verbose && rx_native_format != nullptr) {
        std::cout << "RX native format: " << rx_native_format
                  << " full_scale=" << rx_full_scale << "\n";
        SoapySDR_free(rx_native_format);
    } else if (rx_native_format != nullptr) {
        SoapySDR_free(rx_native_format);
    }
    if (verbose) {
        std::cout << "\nrx_mtu = " << ctx.mtu << "\n";
    }

    if (ctx.mtu == 0) {
        close_rx_stream(ctx);
        return false;
    }

    return true;
}

std::vector<CF> read_rx_samples(
    RxStreamContext& ctx,
    size_t iteration_count,
    size_t warmup_reads,
    bool verbose,
    long timeout_us)
{
    std::vector<CF> received_samples;
    if (ctx.stream == nullptr || ctx.mtu == 0) {
        return received_samples;
    }

    std::vector<CF> rx_cf32(ctx.mtu);

    for (size_t buffers_read = 0; buffers_read < iteration_count + warmup_reads; ++buffers_read) {
        void* rx_buffs[] = {rx_cf32.data()};
        int flags = 0;
        long long timeNs = 0;

        const int sr = SoapySDRDevice_readStream(
            ctx.sdr,
            ctx.stream,
            rx_buffs,
            ctx.mtu,
            &flags,
            &timeNs,
            timeout_us
        );
        if (sr < 0) {
            if (verbose) {
                std::cerr << "RX stream error: " << sr << "\n";
            }
            continue;
        }

        if (buffers_read >= warmup_reads) {
            received_samples.insert(
                received_samples.end(),
                rx_cf32.begin(),
                rx_cf32.begin() + static_cast<ptrdiff_t>(sr)
            );
        }
    }

    return received_samples;
}

void close_rx_stream(RxStreamContext& ctx)
{
    if (ctx.stream == nullptr || ctx.sdr == nullptr) {
        return;
    }

    SoapySDRDevice_deactivateStream(ctx.sdr, ctx.stream, 0, 0);
    SoapySDRDevice_closeStream(ctx.sdr, ctx.stream);
    ctx.stream = nullptr;
    ctx.mtu = 0;
}
