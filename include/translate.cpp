#include "translate.h"

void ModeTX(SoapySDRDevice *sdr, std::vector<CF>& tx_array, size_t iteration_count) { 

    const u_int sample_rate = 1e6;
    const u_int carrier_freq = 800e6;

    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

    size_t channels[] = {0};
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -50.0); // Усиление TX
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    if (txStream == nullptr || tx_array.empty()) {
        std::cerr << "TX stream setup failed or TX frame is empty" << std::endl;
        return;
    }
    
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);

    const size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);
    
    std::vector<int16_t> tx_cs16;
    to_cs16(tx_array, tx_cs16);
    
    std::cout <<  " tx_mtu = " << tx_mtu << std::endl;

    const long timeoutUs = 400000;
    int flags;
    long long timeNs = SoapySDRDevice_getHardwareTime(sdr, 0) + (6 * 1000 * 1000);
    if (timeNs < 0) {
        timeNs = 6 * 1000 * 1000;
    }

    const long long sample_period_ns = 1000000000LL / sample_rate;

    if (iteration_count == 0) {
        iteration_count = 1;
    }

    size_t offset_samples = 0;
    std::vector<int16_t> tx_chunk(2 * tx_mtu);
    for (size_t tx_iter = 0; tx_iter < iteration_count; ++tx_iter) {
        const size_t remaining = tx_array.size() - offset_samples;
        const size_t chunk_samples = std::min(tx_mtu, remaining);
        std::copy_n(
            tx_cs16.begin() + static_cast<ptrdiff_t>(2 * offset_samples),
            static_cast<ptrdiff_t>(2 * chunk_samples),
            tx_chunk.begin()
        );
        const void *tx_buffs[] = {tx_chunk.data()};
        
        flags = SOAPY_SDR_HAS_TIME;
        int st = SoapySDRDevice_writeStream(
            sdr,
            txStream,
            tx_buffs,
            chunk_samples,
            &flags,
            timeNs,
            timeoutUs
        );
        if (st < 0) {
            printf("TX error: %d\n", st);
            break;
        }
        if (st == 0) {
            continue;
        }

        timeNs += static_cast<long long>(st) * sample_period_ns;
        offset_samples += static_cast<size_t>(st);
        if (offset_samples >= tx_array.size()) {
            offset_samples = 0;
        }
    }

    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, txStream);

    
    printf("Часть ModeTX завершена успешно\n");
}
