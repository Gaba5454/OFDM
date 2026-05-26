#include "cfo_functions.h"

double estimate_cfo(
    const std::vector<CF>& symbol_with_cp, 
    size_t n_fft, 
    size_t n_cp,
    double min_magnitude)
{
    // Валидация входа
    if (symbol_with_cp.size() < n_fft + n_cp || n_cp == 0 || n_fft == 0) {
        return NAN;  // сигнал об ошибке
    }

    CF sum(0.0, 0.0);
    
    // Корреляция CP с хвостом символа
    for (size_t i = 0; i < n_cp; ++i) {
        const CF& val_cp   = symbol_with_cp[i];
        const CF& val_tail = symbol_with_cp[n_fft + i];
        sum += val_cp * std::conj(val_tail);
    }

    // Проверка на надёжность (защита от шума)
    if (std::abs(sum) < min_magnitude) {
        return NAN;  // сигнал слишком слабый
    }

    // Вычисление фазового сдвига и конверсия в частоту
    double avg_phase = std::arg(sum);
    return avg_phase / (2.0 * M_PI * static_cast<double>(n_fft));
}

/**
 * @brief Компенсирует частотную ошибку (CFO) в потоке данных.
 * 
 * Применяет фазовый поворот: y[n] = x[n] * exp(-j * 2π * CFO_norm * n)
 * Фаза накапливается непрерывно через весь поток (не сбрасывается на символах).
 * 
 * @param rx_data Принятый поток (вектор комплексных отсчётов).
 * @param cfo_normalized Нормированная ошибка частоты (из estimate_cfo).
 *                       Единицы: доля от поднесущей (1/FFT).
 * @param n_fft Длина FFT (полезная часть символа).
 * @param n_cp Длина циклического префикса.
 * @return std::vector<CF> Исправленный поток того же размера.
 * 
 * @note Если cfo_normalized = 0 или NaN — возвращается копия входа.
 */
std::vector<CF> compensate_cfo(
    const std::vector<CF>& rx_data, 
    double cfo_normalized,
    size_t n_fft, 
    size_t n_cp)
{
    // Быстрые пути: пустой вход или нулевая коррекция
    if (rx_data.empty() || cfo_normalized == 0.0) {
        return rx_data;
    }
    
    // Защита от некорректной оценки частоты
    if (std::isnan(cfo_normalized) || std::isinf(cfo_normalized)) {
        return rx_data;  // или можно вернуть пустой вектор как сигнал ошибки
    }

    const size_t total_samples = rx_data.size();
    std::vector<CF> corrected;
    corrected.reserve(total_samples);  // reserve + push_back чуть безопаснее, чем resize

    // Шаг фазы на один отсчёт (предвычисляем, чтобы не считать в цикле)
    const double phase_step = -2.0 * M_PI * cfo_normalized;
    
    // Накопитель фазы — НЕ сбрасывается между символами!
    double accumulated_phase = 0.0;

    for (size_t idx = 0; idx < total_samples; ++idx) {
        // Компенсируем: умножаем на e^(-j * accumulated_phase)
        corrected.emplace_back(
            rx_data[idx] * CF(std::cos(accumulated_phase), 
                              std::sin(accumulated_phase))
        );
        
        // Накручиваем фазу для следующего отсчёта
        accumulated_phase += phase_step;
        
    }

    return corrected;
}