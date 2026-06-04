#include "another_functions.h"

void to_cs16(const std::vector<CF>& src, std::vector<int16_t>& dst) {
    dst.resize(src.size() * 2);  
    float peak = 0.0f;
    for (const auto& sample : src) {
        peak = std::max(peak, std::abs(sample.real()));
        peak = std::max(peak, std::abs(sample.imag()));
    }

    const float scale = (peak > 0.95f) ? (0.95f / peak) : 1.0f;

    for (size_t i = 0; i < src.size(); ++i) {
        float re = std::clamp(src[i].real() * scale, -1.0f, 1.0f) * 32767.0f;
        float im = std::clamp(src[i].imag() * scale, -1.0f, 1.0f) * 32767.0f;
        dst[2*i]   = static_cast<int16_t>(std::lround(re));
        dst[2*i+1] = static_cast<int16_t>(std::lround(im));
    }
}


std::string bits_to_string(const std::vector<int8_t>& bits) {
    std::string text;
    size_t num_bytes = bits.size() / 8;
    
    for (size_t i = 0; i < num_bytes; ++i) {
        unsigned char byte = 0;
        for (int b = 0; b < 8; ++b) {
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

std::vector<CF> extractDataAfterPSS(
    size_t peak_pos, 
    const std::vector<CF>& rx_array,
    size_t data_len) 
{
    if (peak_pos == SIZE_MAX) {
        return {};
    }

    const size_t pss_total_len = SYMBOL_LEN;
    if (peak_pos > rx_array.size() || pss_total_len > rx_array.size() - peak_pos) {
        return {};
    }

    const size_t start = peak_pos + pss_total_len;
    if (start > rx_array.size() || data_len > rx_array.size() - start) {
        return {}; 
    }

    return {
        rx_array.begin() + static_cast<ptrdiff_t>(start),
        rx_array.begin() + static_cast<ptrdiff_t>(start + data_len)
    };
}


DecodedResult decode_ofdm_stream(const std::vector<CF>& data_fixed, size_t n_fft, size_t n_cp) {
    DecodedResult result;
    const size_t symbol_len = n_fft + n_cp;
    const size_t total_symbols = data_fixed.size() / symbol_len;
    const CF known_pilot(0.707f, 0.707f);
    
    if (total_symbols == 0) return result;

    std::array<bool, 128> is_pilot{};
    is_pilot.fill(false);
    const std::vector<size_t> pilot_indices{28, 38, 48, 58, 68, 78, 88, 98};
    for (size_t p : pilot_indices) {
        is_pilot[p] = true;
    }

    std::vector<size_t> data_indices;
    for (int i = 28; i <= 63; ++i) {
        if (!is_pilot[i]) data_indices.push_back(i);
    }
    for (int i = 65; i <= 100; ++i) {
        if (!is_pilot[i]) data_indices.push_back(i);
    }



    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n_fft);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n_fft);
    fftw_plan plan = fftw_plan_dft_1d(n_fft, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    std::vector<int8_t> all_bits;
    result.constellation_points.reserve(total_symbols * data_indices.size());
    std::vector<CF> freq_symbol(n_fft);

    for (size_t s = 0; s < total_symbols; ++s) {
        size_t start_idx = s * symbol_len;
        if (start_idx + symbol_len > data_fixed.size()) break;

        for (size_t k = 0; k < n_fft; ++k) {
            in[k][0] = data_fixed[start_idx + n_cp + k].real();
            in[k][1] = data_fixed[start_idx + n_cp + k].imag();
        }


        fftw_execute(plan);

        for (size_t idx = 0; idx < n_fft; ++idx) {
            freq_symbol[idx] = CF(out[idx][0], out[idx][1]);
            freq_symbol[idx] /= static_cast<float>(n_fft);
        }

        std::vector<CF> equalized_symbol = equalize_with_pilots(freq_symbol, pilot_indices, known_pilot);

        for (size_t idx : data_indices) { 
            CF freq_sample = equalized_symbol[idx];

            result.constellation_points.push_back(freq_sample);
            auto bits = qpsk_demodulate_symbol(freq_sample);
            all_bits.insert(all_bits.end(), bits.begin(), bits.end());
        }
    }

    result.recovered_text = bits_to_string(all_bits);
    result.symbols_processed = total_symbols;

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return result;
}

std::vector<CF> buildTxFrame(
    size_t num_iterations,
    const std::vector<CF>& pss_symbol,
    const std::vector<CF>& data_symbol,
    size_t pss_period) 
{
    // Быстрая проверка на пустой вход
    if (num_iterations == 0 || pss_symbol.empty() || data_symbol.empty()) {
        return {};
    }

    const size_t estimated_size = num_iterations * data_symbol.size();
    std::vector<CF> frame;
    frame.reserve(estimated_size);

    for (size_t i = 0; i < num_iterations; ++i) {
        if (i % pss_period == 0) {
            frame.insert(frame.end(), pss_symbol.begin(), pss_symbol.end());
        } else {
            frame.insert(frame.end(), data_symbol.begin(), data_symbol.end());
        }
    }

    return frame;
}

void print_usage(const char* prog_name) {
    std::cout << "Usage:\n"
              << "  " << prog_name << " simulation              # Запустить симуляцию с визуализацией\n"
              << "  " << prog_name << " TX <device>             # Передать сигнал через SDR\n"
              << "  " << prog_name << " RX <device>             # Принять сигнал через SDR\n"
              << "\nExamples:\n"
              << "  " << prog_name << " simulation\n"
              << "  " << prog_name << " TX usb:1.7.5\n"
              << "  " << prog_name << " RX usb:1.8.5\\n";
}
