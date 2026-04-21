// functions.cpp
#include "functions.h"

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
