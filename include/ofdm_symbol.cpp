#include "ofdm_symbol.h"

using CF = std::complex<float>;

std::vector<CF> ofdm(const std::vector<CF>& in_sym) {

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

    std::vector<size_t> ind_pilots{28, 38, 48, 58, 68, 78, 88, 98};
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
        if(i == 28 || i == 38 || i == 48 || i == 58 || i == 68 || i == 78 || i == 88 || i == 98) {
            continue;
        }
        if (sym_idx < in_sym.size()) {
            in[i][0] = in_sym[sym_idx].real();
            in[i][1] = in_sym[sym_idx].imag();
            ++sym_idx; 
        } else {
            in[i][0] = 0.0;
            in[i][1] = 0.0;
            ++sym_idx; 
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
        out_sig[i] = CF(out[i][0],out[i][1]);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return out_sig;
}