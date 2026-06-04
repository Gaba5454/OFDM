#include "receive.h"


std::vector<CF> ModeRX(SoapySDRDevice *sdr, size_t iteration_count) {
    
    const u_int sample_rate = 1e6;
    const u_int carrier_freq = 800e6;
    std::vector<CF> received_samples;
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);
    double actual = SoapySDRDevice_getFrequency(sdr, SOAPY_SDR_RX, 0);
    std::cerr << "[MODE_RX] Freq set: " << (carrier_freq/1e6) 
            << " MHz, Actual: " << (actual/1e6) << " MHz" << std::endl;
    size_t channels[] = {0};
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 70.0);  // Усиление RX
    
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    if (rxStream == nullptr) {
        std::cerr << "RX stream setup failed" << std::endl;
        return received_samples;
    }

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
    
    const size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    printf("\nrx_mtu = %zu\n", rx_mtu);
    
    if (rx_mtu == 0) {
        SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
        SoapySDRDevice_closeStream(sdr, rxStream);
        return received_samples;
    }

    std::vector<int16_t> rx_cs16(2 * rx_mtu);
    
    const long timeoutUs = 400000;

    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
        
        void *rx_buffs[] = {rx_cs16.data()};
        int flags;
        long long timeNs;

        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        if (sr < 0) {
            std::cerr << "RX stream error: " << sr << std::endl;
            continue;
        }

        for (size_t i = 0; i < static_cast<size_t>(sr); ++i) {
            float re = static_cast<float>(rx_cs16[2*i]) / 32768.0f;
            float im = static_cast<float>(rx_cs16[2*i + 1]) / 32768.0f;
            received_samples.emplace_back(re, im);
        }
    }

    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, rxStream);

    
    printf("Часть ModeRX завершена успешно\n");

    return received_samples;
}
