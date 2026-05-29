#include "receive.h"


std::vector<CF> ModeRX(SoapySDRDevice *sdr, size_t iteration_count) {
    
    int sample_rate = 1e6;
    int carrier_freq = 800e6;
    std::vector<CF> received_samples;
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);
    
    size_t channels[] = {0};
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 50.0);  // Усиление RX
    
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
    
    // === Получение размеров буферов ===
    const size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    printf("\nrx_mtu = %zu\n", rx_mtu);
    
    int16_t rx_cs16[2 * rx_mtu];
    
    const long timeoutUs = 400000;
    long long last_time = 0;

    // Цикл обработки
    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
        
        void *rx_buffs[] = {rx_cs16};
        
        
        int flags;
        long long timeNs;
        // Конвертация CS16 → CF (нормализация в диапазон [-1.0, 1.0])

        // 1. Чтение из SDR
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        // Конвертация CS16 → CF (нормализация в диапазон [-1.0, 1.0])
        for (size_t i = 0; i < sr; ++i) {
            float re = static_cast<float>(rx_cs16[2*i]) / 32768.0f;
            float im = static_cast<float>(rx_cs16[2*i + 1]) / 32768.0f;
            received_samples.emplace_back(re, im);
        }
    }

    // === Очистка SDR ===
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_unmake(sdr);
    
    printf("Часть ModeRX завершена успешно\n");

    return received_samples;
}