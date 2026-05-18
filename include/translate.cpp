#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "const.h"

void to_cs16(const std::vector<CF>& src, std::vector<int16_t>& dst) {
    dst.resize(src.size() * 2);  
    for (size_t i = 0; i < src.size(); ++i) {
        float re = std::clamp(src[i].real(), -1.0f, 1.0f) * 32767.0f;
        float im = std::clamp(src[i].imag(), -1.0f, 1.0f) * 32767.0f;
        dst[2*i]   = static_cast<int16_t>(std::lround(re));
        dst[2*i+1] = static_cast<int16_t>(std::lround(im));
    }
}

void ModeTX(SoapySDRDevice *sdr, std::vector<CF>& tx_array, size_t iteration_count) { 

    // Параметры среды передачи
    const u_int sample_rate = 1e6;
    const u_int carrier_freq = 800e6;
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

    // Параметры для железа SDR
    size_t channels[] = {0};
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -20.0); // Усиление TX
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);

    // Получение размера буфера
    const size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);
    
    // Конвертация CF в CS
    std::vector<int16_t> tx_cs16;
    to_cs16(tx_array, tx_cs16);
    const size_t samples_to_send = std::min(tx_array.size(), tx_mtu);
    
    std::cout <<  " tx_mtu = " << tx_mtu << std::endl;
    std::vector<CF> tx_buff = tx_array;
    
    // ============ Здесь нужно давать отрисовывать imgui

    const long timeoutUs = 400000;
    long long last_time = 0;
    int flags;
    long long timeNs;
    // Цикл обработки
    size_t buffer_TX = 0;
    if(iteration_count == 0) {
        iteration_count = 1000000;
    }

    while (buffer_TX != iteration_count) 
    {
        long long tx_time = timeNs + (6 * 1000 * 1000);

        void *tx_buffs[] = {tx_buff.data()};
        
        flags = SOAPY_SDR_HAS_TIME;
        int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, 
                                           tx_mtu, &flags, tx_time, timeoutUs);
        if (st < 0) {
            printf("TX error: %d\n", st);
        }

        // Логи
        printf("Buffer: %lu - TimeDiff: %lli ns\n\n", buffer_TX, timeNs - last_time);
        last_time = timeNs;


        buffer_TX++;
        
    }
        // === Очистка SDR ===
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, txStream);
    SoapySDRDevice_unmake(sdr);
    
    printf("Часть ModeTX завершена успешно\n");

}
