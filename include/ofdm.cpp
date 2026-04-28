// ofdm.cpp
#include "const.h"

using CD = std::complex<double>;

std::vector<CD> OFDM(const std::vector<CD>& in_sym) {

    /* 
    *  Нули
    *  С 0 по 27, на 64, с 101 по 127 
    */
    /* 
    *  Пилоты
    *  28, 38, 48, 58, 68, 78, 88, 98
    */

    std::vector<size_t> ind_pilots{28, 38, 48, 58, 68, 78, 88, 98};

    fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * LTE);

    for(size_t i = 0; i < LTE; ++i) {
        in[i][0] = 0.0;
        in[i][1] = 0.0;
    }

    size_t sym_idx = 0; 

    // Add pilots in symbol
    for(size_t i : ind_pilots) {
                in[i][0] = 0.707;
                in[i][1] = 0.707;
    }
    
    // Add data in symbol
    for (size_t i = 28; i <= 100 && sym_idx < in_sym.size(); ++i) {
        if(i == 64){
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