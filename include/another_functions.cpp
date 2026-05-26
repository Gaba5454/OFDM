#include "another_functions.h"

void to_cs16(const std::vector<CF>& src, std::vector<int16_t>& dst) {
    dst.resize(src.size() * 2);  
    for (size_t i = 0; i < src.size(); ++i) {
        float re = std::clamp(src[i].real(), -1.0f, 1.0f) * 32767.0f;
        float im = std::clamp(src[i].imag(), -1.0f, 1.0f) * 32767.0f;
        dst[2*i]   = static_cast<int16_t>(std::lround(re));
        dst[2*i+1] = static_cast<int16_t>(std::lround(im));
    }
}

// 2. Преобразование вектора битов в строку
std::string bits_to_string(const std::vector<int8_t>& bits) {
    std::string text;
    size_t num_bytes = bits.size() / 8;
    
    for (size_t i = 0; i < num_bytes; ++i) {
        unsigned char byte = 0;
        for (int b = 0; b < 8; ++b) {
            // Сдвигаем биты. Важно: порядок битов должен совпадать с string_to_bits
            // В string_to_bits мы брали старший бит первым (MSB first).
            // Значит, bits[i*8 + 0] — это самый старший бит байта.
            byte |= (bits[i * 8 + b] << (7 - b));
        }
        text += static_cast<char>(byte);
    }
    return text;
}

std::vector<uint8_t> string_to_bits(const std::string& text) {
    std::vector<uint8_t> bits(text.size() * 8);
    size_t idx = 0;
    for (unsigned char c : text) {
        for (int i = 7; i >= 0; --i) {
            bits[idx++] = (c >> i) & 1;
        }
    }
    return bits;
}



/**
 * @brief Извлекает фиксированный блок данных после PSS.
 * @param data_len Сколько отсчётов извлечь после PSS-блока.
 */
std::vector<CF> extractDataAfterPSS(
    size_t peak_pos, 
    const std::vector<CF>& rx_array,
    size_t data_len) 
{
    if (peak_pos + LTE + data_len > rx_array.size()) {
        return {}; 
    }
    
    const size_t start = peak_pos + LTE;
    return {
        rx_array.begin() + static_cast<ptrdiff_t>(start),
        rx_array.begin() + static_cast<ptrdiff_t>(start + data_len)
    };
}


DecodedResult decode_ofdm_stream(const std::vector<CF>& data_fixed, size_t n_fft, size_t n_cp) {
    DecodedResult result;
    const size_t symbol_len = n_fft + n_cp;
    const size_t total_symbols = data_fixed.size() / symbol_len;
    
    if (total_symbols == 0) return result;

    // Буферы для FFTW
    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n_fft);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n_fft);
    
    // План FFT (прямое преобразование: время -> частота)
    fftw_plan plan = fftw_plan_dft_1d(n_fft, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    std::vector<int8_t> all_bits;
    result.constellation_points.reserve(total_symbols * 62); // Резервируем место

    for (size_t s = 0; s < total_symbols; ++s) {
        size_t start_idx = s * symbol_len;
        
        // Проверка границ
        if (start_idx + symbol_len > data_fixed.size()) break;

        // --- А. Удаление CP ---
        // Копируем только полезные данные (пропускаем первые n_cp отсчётов)
        for (size_t k = 0; k < n_fft; ++k) {
            in[k][0] = data_fixed[start_idx + n_cp + k].real();
            in[k][1] = data_fixed[start_idx + n_cp + k].imag();
        }

        // --- Б. FFT ---
        fftw_execute(plan);

        // --- В. Извлечение активных поднесущих ---
        // В LTE-like структуре активные поднесущие находятся в центре.
        // Индексы: [32..62] и [65..95] (всего 62 штуки), пропуская DC (индекс 64).
        // Но после FFTW индекс 0 — это DC.
        // Нам нужно маппить индексы правильно.
        
        // DC находится в out[0] (или out[n_fft/2], зависит от реализации, но FFTW дает DC в [0]).
        // Давай возьмем индексы, соответствующие тем, куда мы клали данные в OFDM().
        // Там мы использовали индексы 32..62 и 65..95 в частотном векторе ДО IFFT.
        // После IFFT и FFT обратно, данные вернутся на те же места.
        
        std::vector<size_t> active_indices;
        for(int i=1; i<=62; ++i) active_indices.push_back(i);

        for (size_t idx = 1; idx < n_fft; ++idx) { // Пропускаем только DC (индекс 0)
            CF freq_sample(out[idx][0], out[idx][1]);
            freq_sample /= static_cast<double>(n_fft);
            
            // Если амплитуда слишком мала, пропускаем (шум)
            if (std::abs(freq_sample) < 0.1) continue; 

            result.constellation_points.push_back(freq_sample);
            std::vector<int8_t> bits = qpsk_demodulate_symbol(freq_sample);
            all_bits.insert(all_bits.end(), bits.begin(), bits.end());
        }
    }

    // --- Г. Декодирование битов в текст ---
    result.recovered_text = bits_to_string(all_bits);
    result.symbols_processed = total_symbols;

    // Очистка
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return result;
}