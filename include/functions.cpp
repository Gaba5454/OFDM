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
std::vector<CD> cyclicPrefix(const std::vector<CD>& symbol, size_t cp_len) {

    if(cp_len >= symbol.size()){
        throw std::invalid_argument("Длина должна быть меньше размера символа.");
    }

    std::vector<CD> output;

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
std::vector<CD> channelSimulation(const std::vector<CD>& arrayForTx, size_t arr_len, double noise_stddev = 1.0) {
    
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
    std::vector<CD> result;
    result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
        // Создаём CD из действительной и мнимой части
        // Если CD — это std::complex<double>, этот код тоже сработает
        result.emplace_back(arr_channel[i][0], arr_channel[i][1]);
    }
     // 2. Освобождение памяти FFTW (обязательно!)
    fftw_free(arr_channel);

    return result;
}

/*
* Generate PSS with ZadoffChu algorithm
*/
std::vector<CD> ZadoffChu(int U){
    std::vector<CD> d(62);
    double epsilon = 0.001;

    for(size_t i = 0; i < 30; i++){
        d[i] = exp((-j * M_PI * CD(U) * CD(i) * CD(i+1)) / CD(63));
        if(std::abs(imag(d[i]) < epsilon)) { d[i] = CD(real(d[i]),0); }
    }
    for(size_t i = 31; i < 61; i++){
        d[i] = exp((-j * M_PI * CD(U) * CD(i+1) * CD(i+2)) / CD(63));
        if(std::abs(imag(d[i]) < epsilon)) { d[i] = CD(real(d[i]),0); }
    }
    return d;
}

/*
* Replace first half whith second half for correct TX power spectrum
*/
std::vector<CD> powerShift(std::vector<CD>& arrayOFDM) {
    std::vector<CD> shiftedArr(arrayOFDM.size());
    int n = arrayOFDM.size();
    int mid = (n + 1) / 2;

    for(int i = 0; i < n; i++){
        shiftedArr[i] = arrayOFDM[(i + mid) % n];
    }

    return arrayOFDM;
}

/*
* NID - PSS type
*/
std::vector<CD> PSS(size_t NID) {
    /*  Нули
    *   С 0 по 31, на 64, с 96 по 127 
    */

    std::vector<CD> pssArr;
    if      (NID == 0) {
            pssArr = ZadoffChu(25);
    }
    else if (NID == 1) {
            pssArr = ZadoffChu(29);
    }
    else if (NID == 2) {
            pssArr = ZadoffChu(34);
    }
    std::vector<CD> shift_pssArr = powerShift(pssArr);

    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);

    size_t sym_idx = 0; 

    // Левая часть
    for (size_t i = 31; i <= 63 && sym_idx < LTE; ++i) {
        if (sym_idx < shift_pssArr.size()) {
            in[i][0] = shift_pssArr[sym_idx].real();
            in[i][1] = shift_pssArr[sym_idx].imag();
            ++sym_idx; 
        } else {
            // Если данные кончились — явно зануляем
            in[i][0] = 0.0;
            in[i][1] = 0.0;
        }
    }

    // Правая часть
    for (size_t i = 65; i <= 96 && sym_idx < shift_pssArr.size(); ++i) {

        if (sym_idx < shift_pssArr.size()) {
            in[i][0] = shift_pssArr[sym_idx].real();
            in[i][1] = shift_pssArr[sym_idx].imag();
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

    std::vector<CD> out_sig(LTE);
    for(size_t i = 0; i < LTE; ++i){
        out_sig[i] = CD(out[i][0],
                        out[i][1]);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return out_sig;
}

std::vector<CD> correlation(std::vector<CD>& RxArray, std::vector<CD>& PSS) {
    
}