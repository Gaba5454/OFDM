#include "../include/functions.h"
#include "../include/modulations.h"
#include "../include/ofdm.h"
#include "../include/gui.h"
#include <iostream>
#include <algorithm>
#include <vector>



int main() {
        // 1. Входные данные
        std::string text = "BUREAU1440";
        std::cout << "Processing text: \"" << text << "\"" << std::endl;

        // 2. Преобразование в биты
        std::vector<int8_t> raw_bits = string_to_bits(text);

        // 3. Модуляция QPSK
        std::vector<CD> symbols = QPSK(raw_bits);

        // 4. Генерация PSS (NID=1 == root index 29)
        std::vector<CD> pssSignal = PSS(1);

        // 5. OFDM модуляция данных
        std::vector<CD> ofdm_symbols = OFDM(symbols);

        // 6. Добавление циклического префикса
        std::vector<CD> ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
        std::vector<CD> pss_with_cp = cyclicPrefix(pssSignal, CP_LENGTH);

        // 7. Формирование полного кадра передачи (Tx Array)
        std::vector<CD> array_for_tx;
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
        std::vector<double> corr_map = correlationPSS(array_for_tx, pssSignal);

        // 8.1 Вычисление индекса начала PSS
        int peak_pos = 0;
        if (!corr_map.empty()) {
            auto max_it = std::max_element(corr_map.begin(), corr_map.end());
            peak_pos = std::distance(corr_map.begin(), max_it);
            
            std::cout << "Sync: PSS found at sample index " << peak_pos << std::endl;
        } else {
            std::cerr << "Error: Correlation map is empty!" << std::endl;
        }
        
        // 8.2 Обрезка полезных данных
        std::vector<CD> extracted_data = extractDataAfterPSS(peak_pos, array_for_tx);

//  =========================================
// 9. Частотная синхронизация 
        const size_t SYMBOL_LEN = LTE + CP_LENGTH; // 148

// Сделать обработку случая если данных всего один символ
        if (extracted_data.size() >= SYMBOL_LEN) {
            // Вырезаем первый символ
            std::vector<CD> first_symbol(extracted_data.begin(), extracted_data.begin() + SYMBOL_LEN);

            // 2. Измеряем ошибку частоты
            double cfo = estimate_cfo(first_symbol, LTE, CP_LENGTH);
            
            std::cout << "Detected Frequency Offset (normalized): " << cfo << std::endl;
            // Если всё идеально, cfo будет близко к 0.
            // Если есть рассинхронизация, там будет число вроде 0.001 или -0.005.

            // 3. Исправляем ВЕСЬ поток данных
            std::vector<CD> data_fixed = compensate_cfo(extracted_data, cfo);

// === ДЕКОДИРОВАНИЕ ===
DecodedResult decoded = decode_ofdm_stream(data_fixed, LTE, CP_LENGTH);

// Обрезаем восстановленный текст до длины исходного
std::string clean_text = decoded.recovered_text.substr(0, text.size());

std::cout << "----------------------------------------" << std::endl;
std::cout << "Original Text: \"" << text << "\"" << std::endl;
std::cout << "Recovered Text: \"" << clean_text << "\"" << std::endl;
std::cout << "Match: " << (clean_text == text ? "YES" : "NO") << std::endl;
std::cout << "----------------------------------------" << std::endl;
// === ВИЗУАЛИЗАЦИЯ ===
// Передаем восстановленный текст и точки созвездия в GUI
// Тебе нужно будет обновить сигнатуру run_gui, чтобы принять эти новые данные.
// Или просто передать decoded.constellation_points вместо старых qpsk_symbols, 
// чтобы увидеть "реальное" принятое созвездие.

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
            peak_pos,           // peak_position 
            extracted_data,     // extracted_data
            decoded.recovered_text, 
            decoded.constellation_points
        );
    }


    return 0;
}