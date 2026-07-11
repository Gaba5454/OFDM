#include "ofdm_symbol.h"

#include "fftw_guard.h"
#include "frame_layout.h"

#include <fftw3.h>

std::vector<CF> ofdm(const std::vector<CF>& in_sym)
{
    std::lock_guard<std::mutex> lock(fftw_global_mutex());

    std::vector<CF> centered_bins(LTE, CF(0.0f, 0.0f));
    fftw_complex* in = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * LTE));
    fftw_complex* out = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * LTE));
    const std::vector<size_t>& pilot_indices = pilot_subcarrier_indices();
    const std::vector<size_t>& data_indices = payload_data_indices();

    for (size_t i : pilot_indices) {
        centered_bins[i] = KNOWN_PILOT;
    }

    for (size_t sym_idx = 0; sym_idx < in_sym.size() && sym_idx < data_indices.size(); ++sym_idx) {
        centered_bins[data_indices[sym_idx]] = in_sym[sym_idx];
    }

    for (size_t i = 0; i < LTE; ++i) {
        const size_t unshifted_idx = (i + (LTE / 2)) % LTE;
        in[i][0] = centered_bins[unshifted_idx].real();
        in[i][1] = centered_bins[unshifted_idx].imag();
    }

    fftw_plan plan = fftw_plan_dft_1d(LTE, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);

    fftw_execute(plan);

    std::vector<CF> out_sig(LTE);
    for (size_t i = 0; i < LTE; ++i) {
        out_sig[i] = CF(out[i][0], out[i][1]);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return out_sig;
}
