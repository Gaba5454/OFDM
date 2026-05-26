#include "channel_simulate.h"

/**
 * @brief Добавляет комплексный AWGN-шум к сигналу (модель канала).
 * 
 * Модель: y = x + n, где n ~ CN(0, noise_stddev^2) — комплексный гауссов шум.
 * 
 * @param signal Входной сигнал (вектор комплексных отсчётов).
 * @param noise_stddev Стандартное отклонение шума для каждой компоненты (I и Q).
 * @return std::vector<CF> Сигнал с добавленным шумом (того же размера).
 */
std::vector<CF> channelSimulation(const std::vector<CF>& signal, double noise_stddev) {
    // Если сигнал пустой — нечего обрабатывать
    if (signal.empty()) {
        return {};
    }

    // Инициализация генератора (статический — один на все вызовы)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, noise_stddev);

    std::vector<CF> result;
    result.reserve(signal.size());

    for (const auto& sample : signal) {
        // Генерируем шум для действительной и мнимой части
        double noise_i = dist(gen);
        double noise_q = dist(gen);
        
        // Добавляем шум к сигналу и сохраняем результат
        result.emplace_back(
            static_cast<float>(sample.real() + noise_i),
            static_cast<float>(sample.imag() + noise_q)
        );
    }

    return result;
}