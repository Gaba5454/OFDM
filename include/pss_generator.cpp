#include "pss_generator.h"

/*
* Generate PSS with ZadoffChu algorithm
*/
std::vector<CF> ZadoffChu(int U){
    std::vector<CF> d(62);
    double epsilon = 0.001;

    for(size_t i = 0; i < 30; i++){
        d[i] = exp((-j * CF(M_PI) * CF(U) * CF(i) * CF(i+1)) / CF(63));
        if(std::abs(imag(d[i]) < epsilon)) { d[i] = CF(real(d[i]),0); }
    }
    for(size_t i = 31; i < 61; i++){
        d[i] = exp((-j * CF(M_PI) * CF(U) * CF(i+1) * CF(i+2)) / CF(63));
        if(std::abs(imag(d[i]) < epsilon)) { d[i] = CF(real(d[i]),0); }
    }
    return d;
}

/*
* Replace first half whith second half for correct TX power spectrum
*/
std::vector<CF> powerShift(std::vector<CF>& arrayOFDM) {
    std::vector<CF> shiftedArr(arrayOFDM.size());
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
std::vector<CF> primary_synchronization_signal(size_t NID) {
    /*  Нули
    *   С 0 по 31, на 64, с 96 по 127 
    */

    std::vector<CF> pssArr;
    if      (NID == 0) {
            pssArr = ZadoffChu(25);
    }
    else if (NID == 1) {
            pssArr = ZadoffChu(29);
    }
    else if (NID == 2) {
            pssArr = ZadoffChu(34);
    }
    std::vector<CF> shift_pssArr = powerShift(pssArr);

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

    std::vector<CF> out_sig(LTE);
    for(size_t i = 0; i < LTE; ++i){
        out_sig[i] = CF(out[i][0],
                        out[i][1]);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return out_sig;
}

