#include "simulation.h"

void simulation(){
        std::string text = "BUREAU1440";
        std::cout << "Processing text: \"" << text << "\"" << std::endl;

        // Преобразование в биты
        auto raw_bits = string_to_bits(text);

        // Модуляция QPSK
        auto symbols = qpsk(raw_bits);

        // Генерация PSS (NID=1 == root index 29)
        auto pssSignal = primary_synchronization_signal(1);

        // OFDM модуляция данных
        auto ofdm_symbols = ofdm(symbols);

        // Добавление циклического префикса
        auto ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
        auto pss_with_cp = cyclicPrefix(pssSignal, CP_LENGTH);

        auto array_for_tx = buildTxFrame(iter, pss_with_cp, ofdm_with_cp, 5);

        // Искажение кадра средой передачи
        auto bad_array_for_tx = channelSimulation(array_for_tx, 0.2);
        
        // Синхронизация: Корреляция для поиска PSS
        auto corr_map = correlationPSS(bad_array_for_tx, pssSignal);

        auto peak_pos = findCorrelationPeak(corr_map);
        
        if (peak_pos != 0 || !corr_map.empty()) {  // 0 может быть валидным пиком!
            std::cout << "Sync: PSS found at sample index " << peak_pos << std::endl;
        }

        // Обрезка полезных данных
        auto extracted_data = extractDataAfterPSS(peak_pos, bad_array_for_tx, SYMBOL_LEN*3);

        // Частотная синхронизация 

        // Сделать обработку случая если данных всего один символ
        if (extracted_data.size() >= SYMBOL_LEN) {
            // Вырезаем п ервый символ

            std::vector<CF> first_symbol(extracted_data.begin(), extracted_data.begin() + SYMBOL_LEN);

            // 2. Измеряем ошибку частоты
            auto cfo = estimate_cfo(first_symbol, LTE, CP_LENGTH, 1e-3);
            
            std::cout << "Detected Frequency Offset (normalized): " << cfo << std::endl;
            // Если всё идеально, cfo будет близко к 0.
            // Если есть рассинхронизация, там будет число вроде 0.001 или -0.005.

            // 3. Исправляем ВЕСЬ поток данных
            std::vector<CF> data_fixed = compensate_cfo(extracted_data, cfo, LTE, CP_LENGTH);
        
            // === ДЕКОДИРОВАНИЕ ===
            DecodedResult decoded = decode_ofdm_stream(data_fixed, LTE, CP_LENGTH);

            for (int i = 0; i < 8; ++i) {
                CF s = decoded.constellation_points[i];

                int b0 = (s.real() >= 0.0f) ? 1 : 0; 
                int b1 = (s.imag() >= 0.0f) ? 1 : 0;
                std::cout << b0 << b1;
            }
            std::cout << std::endl;
            // Обрезаем восстановленный текст до длины исходного
            std::string clean_text = decoded.recovered_text.substr(0, text.size());

            std::cout << "----------------------------------------" << std::endl;
            std::cout << "Original Text: \"" << text << "\"" << std::endl;
            std::cout << "Recovered Text: \"" << clean_text << "\"" << std::endl;
            std::cout << "Match: " << (clean_text == text ? "YES" : "NO") << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            // 9. Запуск визуализации
            run_gui(
                text,                       // original_text
                raw_bits,                   // raw_bits
                symbols,                    // qpsk_symbols
                pssSignal,                  // pss_signal
                ofdm_symbols,               // ofdm_symbols
                ofdm_with_cp,               // ofdm_with_cp
                bad_array_for_tx,           // tx_array
                corr_map,                   // correlation_map
                peak_pos,                   // peak_position 
                extracted_data,             // extracted_data
                decoded.recovered_text, 
                decoded.constellation_points
            );
    }

}