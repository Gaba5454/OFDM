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

std::vector<double> correlationPSS(const std::vector<CD>& RxArray, const std::vector<CD>& PSS) {

    std::vector<double> corrArr;
    size_t len_rx = RxArray.size();
    size_t len_pss = PSS.size();
    corrArr.reserve(len_rx - len_pss + 1);
    
    for(size_t k = 0; k <= len_rx - len_pss; ++k) {
        CD sum(0.0, 0.0);

        for (size_t n = 0; n < len_pss; ++n) {
            sum += RxArray[n+k]*std::conj(PSS[n]);
        }
        corrArr.push_back(std::abs(sum));
    }
    return corrArr;
}

std::vector<CD> extractDataAfterPSS(size_t peak_pos, std::vector<CD> rx_array){
    
    size_t start_of_next_block = peak_pos + LTE; // 128
    
    if (start_of_next_block >= rx_array.size()) {
        return {}; 
    }

    // Возвращаем всё, что идёт после первого PSS-блока
    std::vector<CD> data_only(
        rx_array.begin() + start_of_next_block,
        rx_array.end()
    );

    return data_only;
} 

// ================================================
/**
 * @brief Оценивает ошибку частоты (CFO) по одному OFDM символу.
 * @param symbol_with_cp Вектор: [CP (20)] + [Data (128)]. Всего 148 элементов.
 * @param n_fft Длина полезных данных (128).
 * @param n_cp Длина CP (20).
 * @return Нормированная ошибка частоты (в долях от частоты дискретизации).
 */
double estimate_cfo(const std::vector<CD>& symbol_with_cp, size_t n_fft, size_t n_cp) {

    if (symbol_with_cp.size() < n_fft + n_cp) return 0.0;

    CD sum(0.0, 0.0);

    // Сравниваем i-й элемент CP с i-м элементом хвоста Data.
    // CP лежит в индексах [0 ... n_cp-1]
    // Хвост Data лежит в индексах [n_fft ... n_fft + n_cp - 1]
    // Data начинается после CP (индекс n_cp), 
    // и длится n_fft отсчётов. Конец Data = n_cp + n_fft - 1.
    // Хвост длиной n_cp берётся с конца Data.
    
    // Индекс начала хвоста внутри вектора:
    // (n_cp + n_fft) - n_cp = n_fft.
    
    for (size_t i = 0; i < n_cp; ++i) {
        // Берём элемент из CP
        CD val_cp = symbol_with_cp[i];
        
        // Берём соответствующий элемент из хвоста Data
        CD val_tail = symbol_with_cp[n_fft + i];
        
        // Умножаем одно на сопряжённое другое, чтобы получить разность фаз
        sum += val_cp * std::conj(val_tail);
    }

    // Находим средний угол поворота фазы
    double angle = std::arg(sum);

    // Переводим угол в частотную ошибку.
    // Формула: CFO_norm = Angle / (2 * PI * N_FFT)
    double cfo_normalized = angle / (2.0 * M_PI * static_cast<double>(n_fft));

    return cfo_normalized;
}

/**
 * @brief Убирает частотную ошибку из всего потока данных.
 * @param rx_data Принятый поток данных (без PSS, только данные).
 * @param cfo_normalized Ошибка, полученная из estimate_cfo.
 * @return Исправленный поток данных.
 */
std::vector<CD> compensate_cfo(const std::vector<CD>& rx_data, double cfo_normalized) {
    std::vector<CD> corrected(rx_data.size());

    for (size_t i = 0; i < rx_data.size(); ++i) {
        // Мы должны умножить сигнал на e^(-j * 2 * pi * CFO * t)
        // Где t — это номер отсчёта (i).
        
        double phase = -2.0 * M_PI * cfo_normalized * static_cast<double>(i);
        
        // Создаём комплексное число для поворота фазы
        CD correction(std::cos(phase), std::sin(phase));
        
        // Применяем поправку
        corrected[i] = rx_data[i] * correction;
    }

    return corrected;
}


// 1. QPSK Демодулятор: Комплексное число -> 2 бита (int8_t)
std::vector<int8_t> qpsk_demodulate_symbol(const CD& symbol) {
    std::vector<int8_t> bits(2);
    
    // Вариант: Инвертируем логику (так как у тебя bit0==0 -> -1)
    // Если real > 0, то это бит 1. Если imag > 0, то это бит 1.
    // Но возможно, биты идут в порядке [Q, I] или наоборот.
    
    // Попробуем стандартный маппинг для твоего QPSK (bit0->I, bit1->Q):
    // У тебя: bit0=0 -> I=-1. Значит, если I > 0, то bit0 должен быть 1.
    bits[0] = (symbol.real() > 0) ? 1 : 0;
    bits[1] = (symbol.imag() > 0) ? 1 : 0;
    
    return bits;
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



DecodedResult decode_ofdm_stream(const std::vector<CD>& data_fixed, size_t n_fft, size_t n_cp) {
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
     CD freq_sample(out[idx][0], out[idx][1]);
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
// =================================