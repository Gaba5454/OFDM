#include "corellations.h"

/**
 * @brief Вычисляет скользящую корреляцию для детектирования PSS.
 * 
 * Алгоритм: C[k] = |Σ Rx[k+n] * conj(PSS[n])|, n = 0..len_pss-1
 * Результат нормализован на энергию опорного сигнала для устойчивости к усилению.
 * 
 * @param RxArray Принятый сигнал.
 * @param PSS Опорная последовательность PSS.
 * @return std::vector<double> Массив модулей корреляции (размер: len_rx - len_pss + 1).
 */
std::vector<double> correlationPSS(
    const std::vector<CF>& RxArray, 
    const std::vector<CF>& PSS) 
{
    const size_t len_rx = RxArray.size();
    const size_t len_pss = PSS.size();
    
    // Защита от некорректных входных данных
    if (len_pss == 0 || len_rx < len_pss) {
        return {};
    }
    
    // Предварительный расчёт энергии PSS (для нормализации)
    double pss_energy = 0.0;
    for (const auto& s : PSS) {
        pss_energy += std::norm(s);  // |s|^2, быстрее чем abs(s)*abs(s)
    }
    const double pss_norm = std::sqrt(pss_energy);
    
    const size_t output_size = len_rx - len_pss + 1;
    std::vector<double> corrArr;
    corrArr.reserve(output_size);
    
    for (size_t k = 0; k < output_size; ++k) {
        CF sum(0.0, 0.0);
        
        // Накопление корреляции
        for (size_t n = 0; n < len_pss; ++n) {
            sum += RxArray[k + n] * std::conj(PSS[n]);
        }
        
        // Нормализация: деление на |PSS|, чтобы результат не зависел от длины/мощности
        double correlation = std::abs(sum) / pss_norm;
        corrArr.push_back(correlation);
    }
    
    return corrArr;
}

/**
 * @brief Находит позицию максимального значения в карте корреляции.
 * 
 * @param corr_map Вектор значений корреляции (обычно модули комплексных чисел).
 * @return size_t Индекс пика, или 0 если карта пуста.
 */
size_t findCorrelationPeak(const std::vector<double>& corr_map) {
    
    if (corr_map.empty()) {
        std::cerr << "Error: Correlation map is empty!" << std::endl;
        return 0;
    }
    
    auto max_it = std::max_element(corr_map.begin(), corr_map.end());
    return static_cast<size_t>(std::distance(corr_map.begin(), max_it));
}