#include "pss_generator.h"

/*
* Generate PSS with ZadoffChu algorithm
*/
std::vector<CF> ZadoffChu(int U){
    std::vector<CF> d(62);
    constexpr float epsilon = 0.001f;

    for (size_t i = 0; i <= 30; ++i) {
        d[i] = std::exp((-j * CF(static_cast<float>(M_PI)) * CF(static_cast<float>(U)) *
                         CF(static_cast<float>(i)) * CF(static_cast<float>(i + 1))) / CF(63.0f));
        if (std::abs(std::imag(d[i])) < epsilon) {
            d[i] = CF(std::real(d[i]), 0.0f);
        }
    }
    for (size_t i = 31; i < 62; ++i) {
        d[i] = std::exp((-j * CF(static_cast<float>(M_PI)) * CF(static_cast<float>(U)) *
                         CF(static_cast<float>(i + 1)) * CF(static_cast<float>(i + 2))) / CF(63.0f));
        if (std::abs(std::imag(d[i])) < epsilon) {
            d[i] = CF(std::real(d[i]), 0.0f);
        }
    }

    return d;
}

/*
* Replace first half whith second half for correct TX power spectrum
*/
std::vector<CF> powerShift(const std::vector<CF>& arrayOFDM) {
    std::vector<CF> shiftedArr(arrayOFDM.size());
    const size_t n = arrayOFDM.size();
    const size_t mid = n / 2;

    for (size_t i = 0; i < n; ++i) {
        shiftedArr[i] = arrayOFDM[(i + mid) % n];
    }

    return shiftedArr;
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

    for (size_t i = 0; i < LTE; ++i) {
        in[i][0] = 0.0;
        in[i][1] = 0.0;
    }

    size_t sym_idx = 0; 

    for (size_t i = 31; i <= 61 && sym_idx < shift_pssArr.size(); ++i) {
        in[i][0] = shift_pssArr[sym_idx].real();
        in[i][1] = shift_pssArr[sym_idx].imag();
        ++sym_idx;
    }

    for (size_t i = 66; i <= 96 && sym_idx < shift_pssArr.size(); ++i) {
        in[i][0] = shift_pssArr[sym_idx].real();
        in[i][1] = shift_pssArr[sym_idx].imag();
        ++sym_idx;
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
    for (size_t i = 0; i < LTE; ++i) {
        out_sig[i] = CF(static_cast<float>(out[i][0]),
                        static_cast<float>(out[i][1]));
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return out_sig;
}
