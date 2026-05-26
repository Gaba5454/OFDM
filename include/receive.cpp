#include "receive.h"


void ModeRX(SoapySDRDevice *sdr, std::vector<CF>& tx_array, size_t iteration_count) {
    
    int sample_rate = 1e6;
    int carrier_freq = 800e6;
    
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);
    
    size_t channels[] = {0};
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 40.0);  // Усиление RX
    
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
    
    // === Получение размеров буферов ===
    const size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    printf("\nrx_mtu = %zu\n", rx_mtu);
    
    int16_t rx_buffer[2 * rx_mtu];
    
    const long timeoutUs = 400000;
    long long last_time = 0;
    size_t iteration_count = 10;
    
    // Цикл обработки
    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
        
        void *rx_buffs[] = {rx_buffer};
        
        
        int flags;
        long long timeNs;
        
        // 1. Чтение из SDR
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);

    }

    // === Очистка SDR ===
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_unmake(sdr);
    
    printf("Часть ModeRX завершена успешно\n");
}