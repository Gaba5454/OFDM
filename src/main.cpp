// main.cpp
#include <iostream>
#include <fftw3.h>
#include <math.h>
#include <complex>
#include <vector>
#include <bitset>
#include <fstream>
#include <random> // Библиотека для рандомной генерации, которая используется в функции channelSimulation

#define LTE 128

using ComplexSymbol = std::complex<double>;


std::vector<int8_t> string_to_bits(const std::string& text) {
    std::vector<int8_t> bits;
    bits.reserve(text.size() * 8);
    
    for (unsigned char c : text) {  

        for (int i = 7; i >= 0; --i) {
            bits.push_back((c >> i) & 1);
        }
    }
    return bits;
}


std::vector<ComplexSymbol> QPSK(const std::vector<int8_t> &in_bits) {
    
// Переменная количество выходных символов
    size_t num_symbols = in_bits.size() / 2;

// Объявляется переменная для выходного массива, и сразу же удобно 
// под неё выделяется память на количество выходных символов
    std::vector<ComplexSymbol> out_sym;
    out_sym.reserve(num_symbols);

// Цикл преобразования битов в символы, в конце каждой итерации
// Вычисляется финальная координата точки (с нормализацией).
// Конструируется объект std::complex прямо в памяти вектора.
// Увеличивается размер вектора на 1.
    for(size_t i = 0; i < in_bits.size(); i+=2) {
        int8_t bit0 = in_bits[i];
        int8_t bit1 = in_bits[i+1];

        double i_component = (bit0 == 0) ? -1.0f : 1.0f;
        double q_component = (bit1 == 0) ? -1.0f : 1.0f;

        out_sym.emplace_back(i_component, q_component);
    }
    return out_sym;
}


std::vector<ComplexSymbol> OFDM(const std::vector<ComplexSymbol>& in_sym) {

    /* 
    *  Нули
    *  С 0 по 27, на 64, с 101 по 127 
    */
    /* 
    *  Пилоты
    *  28, 38, 48, 58, 68, 78, 88, 98
    */


    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);

    for(size_t i = 0; i < LTE; ++i) {
        in[i][0] = 0.0;
        in[i][1] = 0.0;
    }

    size_t sym_idx = 0; 

    // Левая часть
    for (size_t i = 28; i <= 63 && sym_idx < in_sym.size(); ++i) {
        if (i == 28 || i == 38 || i == 48 || i == 58) {
            in[i][0] = 1.0;
            in[i][1] = 1.0;
            continue;
        }
        if (sym_idx < in_sym.size()) {
            in[i][0] = in_sym[sym_idx].real();
            in[i][1] = in_sym[sym_idx].imag();
            ++sym_idx; 
        } else {
            // Если данные кончились — явно зануляем
            in[i][0] = 0.0;
            in[i][1] = 0.0;
        }
    }

    // Правая часть
    for (size_t i = 65; i <= 100 && sym_idx < in_sym.size(); ++i) {
        if (i == 68 || i == 78 || i == 88 || i == 98) {
            in[i][0] = 1.0;
            in[i][1] = 1.0;
            continue;
        }
        if (sym_idx < in_sym.size()) {
            in[i][0] = in_sym[sym_idx].real();
            in[i][1] = in_sym[sym_idx].imag();
            ++sym_idx; 
        } else {
            // Если данные кончились — явно зануляем
            in[i][0] = 0.0;
            in[i][1] = 0.0;
        }
    }
    fftw_plan plan = fftw_plan_dft_1d(
                                      LTE, 
                                      in, 
                                      out,
                                      FFTW_BACKWARD,
                                      FFTW_ESTIMATE
                                    );
    
    fftw_execute(plan);

    std::vector<ComplexSymbol> out_sig(LTE);
    for(size_t i = 0; i < LTE; ++i){
        out_sig[i] = ComplexSymbol(out[i][0],
                                   out[i][1]);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return out_sig;
}

// Думаю нужно поменять алгоритм работы этой функции, чтобы она была более C подобной
std::vector<ComplexSymbol> cyclicPrefix(const std::vector<ComplexSymbol>& symbol, size_t cp_len) {

    if(cp_len >= symbol.size()){
        throw std::invalid_argument("Длина должна быть меньше размера символа.");
    }

    std::vector<ComplexSymbol> output;

    output.reserve(symbol.size()+cp_len);

    // 1. Копируем последние cp_len отсчётов в начало
    output.insert(output.end(), 
                  symbol.end() - cp_len, 
                  symbol.end());
    
    // 2. Копируем основной символ целиком
    output.insert(output.end(), 
                  symbol.begin(), 
                  symbol.end());

    return output; 
}


/* Функция симуляции канала передачи
@brief Симулирует поведение канала передачи
@param arrayForTx  массив данных на передачу
@param arr_len размер входного массива
@return arr_channel массив данных после обработки
*/
std::vector<ComplexSymbol> channelSimulation(const std::vector<ComplexSymbol>& arrayForTx, size_t arr_len, double noise_stddev = 1.0) {
    
    size_t length = arr_len + arr_len;
    fftw_complex* arr_channel = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * length);
    
    // Инициализация генератора и распределения

    /* Интерфейс к аппаратному генератору случайных чисел (если ОС и железо поддерживают). Он выдаёт непредсказуемые значения, а не псевдослучайные. */
    static std::random_device rd;            

    /*Это название алгоритма: "Вихрь Мерсенна". Он не берёт случайность извне, а вычисляет её пo формуле. rd в данной строке просто seed*/
    static std::mt19937 gen(rd());

    std::normal_distribution<double> dist(0.0, noise_stddev);

    // Заполняем массив комплексным AWGN шумом
    for (size_t i = 0; i < length; ++i) {
        arr_channel[i][0] = dist(gen); // Действительная часть (Re)
        arr_channel[i][1] = dist(gen); // Мнимая часть (Im)
    }
    std::vector<ComplexSymbol> result;
    result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
        // Создаём ComplexSymbol из действительной и мнимой части
        // Если ComplexSymbol — это std::complex<double>, этот код тоже сработает
        result.emplace_back(arr_channel[i][0], arr_channel[i][1]);
    }
     // 2. Освобождение памяти FFTW (обязательно!)
    fftw_free(arr_channel);

    return result;
}


int main() {

        std::string text = "BUREAU1440";
        std::cout << "Исходный текст: \"" << text << "\"" << std::endl;
        std::cout << "Длина: " << text.size() << " символов = " << text.size() * 8 << " бит" << std::endl;
        
        std::vector<int8_t> raw_bits = string_to_bits(text);
        
        std::cout << "\nБиты: ";
        for (size_t i = 0; i < text.size() * 8; i++) {
            std::cout << (int)raw_bits[i];
        }
        std::cout << std::endl;
        
        std::vector<ComplexSymbol> symbols = QPSK(raw_bits);
        int count = 0;
        std::cout << "\nСимволы: ";
        for (size_t i = 0; i < symbols.size(); i++) {
            std::cout << (ComplexSymbol)symbols[i];
            count++;
        }
        std::cout << std::endl << "Всего символов: " << count;
        std::cout << std::endl;

        std::vector<ComplexSymbol> ofdm_symbols = OFDM(symbols);
        int count1 = 0;
        std::cout << "\nОБПФ: ";
        for (size_t i = 0; i < ofdm_symbols.size(); i++) {
            std::cout << (ComplexSymbol)ofdm_symbols[i];
            count1++;
        }
        std::cout << std::endl << count1;
        std::cout << std::endl;

        // === Добавление циклического префикса ===
        const size_t CP_LENGTH = 20;  // ~1/12 от 128
        std::vector<ComplexSymbol> ofdm_with_cp = cyclicPrefix(ofdm_symbols, CP_LENGTH);
        std::cout << "\n✓ Символ с циклическим префиксом: " << ofdm_with_cp.size() << " отсчётов" << std::endl;

       return 0;
}
