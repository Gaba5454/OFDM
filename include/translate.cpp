#include "translate.h"

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

/**
 * @brief Формирует кадр передачи с периодической вставкой синхросигнала (PSS).
 * 
 * Структура кадра: [PSS][DATA][DATA][DATA][DATA][PSS][DATA]...
 * PSS вставляется каждые pss_period символов (по умолчанию — каждый 5-й).
 * 
 * @param num_iterations Общее количество символов в кадре.
 * @param pss_symbol Символ PSS с уже добавленным циклическим префиксом.
 * @param data_symbol Обычный символ данных с циклическим префиксом.
 * @param pss_period Период вставки PSS (по умолчанию 5).
 * @return std::vector<CF> Полный кадр для передачи.
 */
std::vector<CF> buildTxFrame(
    size_t num_iterations,
    const std::vector<CF>& pss_symbol,
    const std::vector<CF>& data_symbol,
    size_t pss_period = 5) 
{
    // Быстрая проверка на пустой вход
    if (num_iterations == 0 || pss_symbol.empty() || data_symbol.empty()) {
        return {};
    }

    // Оцениваем размер: грубая оценка для reserve (все символы считаем как data)
    const size_t estimated_size = num_iterations * data_symbol.size();
    std::vector<CF> frame;
    frame.reserve(estimated_size);

    for (size_t i = 0; i < num_iterations; ++i) {
        // Вставляем PSS каждые pss_period символов (включая 0-й)
        if (i % pss_period == 0) {
            frame.insert(frame.end(), pss_symbol.begin(), pss_symbol.end());
        } else {
            frame.insert(frame.end(), data_symbol.begin(), data_symbol.end());
        }
    }

    return frame;
}