// src/main.cpp
#include "../include/simulation.h"
#include "../include/translate.h"
#include "../include/receive.h"
#include <cstring>
#include <iostream>
#include <string>



int main(int argc, char* argv[]) {
    
    std::string mode = argv[1];
    const char *device_uri = argv[2];

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (mode == "simulation") {
        simulation();
        return 0;
    }
    
    if (mode == "TX" || mode == "RX") {
        if (argc < 3) {
            std::cerr << "Error: Device argument required for " << mode << " mode\n\n";
            print_usage(argv[0]);
            return 1;
        }

        SoapySDRKwargs args = {};
        SoapySDRKwargs_set(&args, "driver", "plutosdr");
        SoapySDRKwargs_set(&args, "uri", device_uri);
        SoapySDRKwargs_set(&args, "direct", "1");
        SoapySDRKwargs_set(&args, "timestamp_every", "1920");
        SoapySDRKwargs_set(&args, "loopback", "0");
        SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
        SoapySDRKwargs_clear(&args);

        if (!sdr) {
            std::cerr << "Failed to open SDR device: " << device_uri << std::endl;
            return 1;
        }
        
        if (mode == "TX") {
            const std::string text = "BUREAU1440";
            const size_t iter = 10;
            
            auto raw_bits = string_to_bits(text);
            auto symbols = qpsk(raw_bits);
            auto pssSignal = primary_synchronization_signal(1);
            auto ofdm_symbols = ofdm(symbols);
            auto ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
            auto pss_with_cp = cyclicPrefix(pssSignal, CP_LENGTH);
            auto tx_frame = buildTxFrame(iter, pss_with_cp, ofdm_with_cp, 5);
            
            std::cout << "TX: Sending " << tx_frame.size() << " samples via " << device_uri << std::endl;
            
            ModeTX(sdr, tx_frame, iter);
            
            std::cout << "TX completed." << std::endl;
            
            
        } 
        if (mode == "RX") {  
            std::cout << "RX: Listening on " << device_uri << " for 100 ms..." << std::endl;
            const size_t iter = 10;
            auto rx_samples = ModeRX(sdr, iter);
            
            if (rx_samples.empty()) {
                std::cerr << "RX: No samples received." << std::endl;
                SoapySDRDevice_unmake(sdr);
                return 1;
            }
            
            std::cout << "RX: Received " << rx_samples.size() << " samples" << std::endl;
            
            const std::string expected_text = "BUREAU1440";
            auto pssSignal = primary_synchronization_signal(1);

            auto corr_map = correlationPSS(rx_samples, pssSignal);
            auto peak_pos = findCorrelationPeak(corr_map);
            
            if (!corr_map.empty()) {
                std::cout << "Sync: PSS found at index " << peak_pos << std::endl;
            }
            
            auto extracted_data = extractDataAfterPSS(peak_pos, rx_samples, SYMBOL_LEN * 3);
            
            if (extracted_data.size() >= SYMBOL_LEN) {

                std::vector<CF> first_symbol(extracted_data.begin(), extracted_data.begin() + SYMBOL_LEN);
                auto cfo = estimate_cfo(first_symbol, LTE, CP_LENGTH, 1e-3);
                std::cout << "CFO: " << cfo << std::endl;
                
                std::vector<CF> data_fixed = compensate_cfo(extracted_data, cfo, LTE, CP_LENGTH);
                
                // Декодирование
                DecodedResult decoded = decode_ofdm_stream(data_fixed, LTE, CP_LENGTH);
                
                // Результат
                std::string clean_text = decoded.recovered_text.substr(0, expected_text.size());
                
                std::cout << "----------------------------------------\n";
                std::cout << "Expected: \"" << expected_text << "\"\n";
                std::cout << "Received: \"" << clean_text << "\"\n";
                std::cout << "Match: " << (clean_text == expected_text ? "YES" : "NO") << "\n";
                std::cout << "----------------------------------------\n";
                
                auto pssSignal = primary_synchronization_signal(1);

                run_gui(
                    expected_text,              // original_text (для сравнения)
                    {},                         // raw_bits (пустой, т.к. не знаем точно, что отправили)
                    {},                         // qpsk_symbols (пустой)
                    pssSignal,                  // pss_signal (эталон для корреляции)
                    {},                         // ofdm_symbols (пустой)
                    {},                         // ofdm_with_cp (пустой)
                    rx_samples,                 // tx_array ← ГЛАВНОЕ: реальный принятый сигнал!
                    corr_map,                   // correlation_map (из rx_samples)
                    peak_pos,                   // peak_position
                    extracted_data,             // данные после извлечения
                    decoded.recovered_text,     // восстановленный текст
                    decoded.constellation_points // созвездие из принятых данных
                );
            }
            
        }
        
        // Очистка SDR
        SoapySDRDevice_unmake(sdr);
        return 0;
    }
    
    // === Неизвестный режим ===
    std::cerr << "Error: Unknown mode '" << mode << "'\n\n";
    print_usage(argv[0]);
    return 1;
}


