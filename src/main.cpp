#include "../include/functions.h"
#include "../include/modulations.h"
#include "../include/ofdm.h"
#include "../include/gui.h"
#include <iostream>
#include <algorithm> // Для std::max_element
#include <vector>



int main() {
    try {
        // 1. Входные данные
        std::string text = "BUREAU1440";
        std::cout << "Processing text: \"" << text << "\"" << std::endl;

        // 2. Преобразование в биты
        std::vector<int8_t> raw_bits = string_to_bits(text);

        // 3. Модуляция QPSK
        std::vector<CD> symbols = QPSK(raw_bits);

        // 4. Генерация PSS (NID=1 -> root index 29)
        std::vector<CD> pssSignal = PSS(1);

        // 5. OFDM модуляция данных
        std::vector<CD> ofdm_symbols = OFDM(symbols);

        // 6. Добавление циклического префикса
        std::vector<CD> ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
        std::vector<CD> pss_with_cp = cyclicPrefix(pssSignal, CP_LENGTH);

        // 7. Формирование полного кадра передачи (Tx Array)
        size_t iter = 10; 
        std::vector<CD> array_for_tx;
        // Резервируем память заранее для производительности
        array_for_tx.reserve(iter * (LTE + CP_LENGTH));

        for(int i = 0; i < iter; i++){
            if (i % 5 == 0){
                // Вставляем PSS каждые 5 символов
                array_for_tx.insert(array_for_tx.end(), pss_with_cp.begin(), pss_with_cp.end());
            }
            else {
                // Иначе вставляем символы с данными
                array_for_tx.insert(array_for_tx.end(), ofdm_with_cp.begin(), ofdm_with_cp.end());
            }
        }

        // 8. Синхронизация: Корреляция для поиска PSS
        // Функция correlation теперь возвращает std::vector<double> (модули корреляции)
        std::vector<double> corr_map = correlation(array_for_tx, pssSignal);

        // Находим позицию максимального пика
        size_t peak_pos = 0;
        if (!corr_map.empty()) {
            auto max_it = std::max_element(corr_map.begin(), corr_map.end());
            peak_pos = std::distance(corr_map.begin(), max_it);
            
            std::cout << "Sync: PSS found at sample index " << peak_pos << std::endl;
        } else {
            std::cerr << "Error: Correlation map is empty!" << std::endl;
        }
        
        // 9. Запуск визуализации
        run_gui(
            text,               // original_text
            raw_bits,           // raw_bits
            symbols,            // qpsk_symbols
            pssSignal,          // pss_signal
            ofdm_symbols,       // ofdm_symbols
            ofdm_with_cp,       // ofdm_with_cp
            array_for_tx,       // tx_array
            corr_map,           // correlation_map
            peak_pos            // peak_position 
        );

    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}