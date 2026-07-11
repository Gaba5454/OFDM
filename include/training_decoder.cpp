#include "training_decoder.h"

#include "equalizer.h"
#include "fftw_guard.h"
#include "frame_layout.h"
#include "payload_codec.h"

#include <algorithm>
#include <cmath>
#include <fftw3.h>
#include <limits>
#include <mutex>

namespace {

constexpr uint8_t kMagic0 = 0x4Fu;
constexpr uint8_t kMagic1 = 0x44u;
constexpr float kLmmseCorrDecay = 12.0f;

using ComplexMatrix = std::vector<std::vector<CF>>;

// FFT caches are reused across calls so the decoder does not recreate FFTW plans
// in the hot RX path.
struct ForwardFftCache {
    fftw_complex* in = nullptr;
    fftw_complex* out = nullptr;
    fftw_plan plan = nullptr;
    size_t size = 0;

    void ensure(size_t n)
    {
        if (plan != nullptr && size == n) {
            return;
        }

        std::lock_guard<std::mutex> lock(fftw_global_mutex());
        if (plan != nullptr) {
            fftw_destroy_plan(plan);
            fftw_free(in);
            fftw_free(out);
        }

        in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
        out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
        plan = fftw_plan_dft_1d(static_cast<int>(n), in, out, FFTW_FORWARD, FFTW_ESTIMATE);
        size = n;
    }
};

struct SmoothFftCache {
    fftw_complex* freq_in = nullptr;
    fftw_complex* time_out = nullptr;
    fftw_complex* time_in = nullptr;
    fftw_complex* freq_out = nullptr;
    fftw_plan ifft_plan = nullptr;
    fftw_plan fft_plan = nullptr;
    size_t size = 0;

    void ensure(size_t n)
    {
        if (ifft_plan != nullptr && fft_plan != nullptr && size == n) {
            return;
        }

        std::lock_guard<std::mutex> lock(fftw_global_mutex());
        if (ifft_plan != nullptr) {
            fftw_destroy_plan(ifft_plan);
            fftw_destroy_plan(fft_plan);
            fftw_free(freq_in);
            fftw_free(time_out);
            fftw_free(time_in);
            fftw_free(freq_out);
        }

        freq_in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
        time_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
        time_in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
        freq_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
        ifft_plan = fftw_plan_dft_1d(
            static_cast<int>(n),
            freq_in,
            time_out,
            FFTW_BACKWARD,
            FFTW_ESTIMATE
        );
        fft_plan = fftw_plan_dft_1d(
            static_cast<int>(n),
            time_in,
            freq_out,
            FFTW_FORWARD,
            FFTW_ESTIMATE
        );
        size = n;
    }
};

ForwardFftCache& forward_fft_cache()
{
    static thread_local ForwardFftCache* cache = new ForwardFftCache();
    return *cache;
}

SmoothFftCache& smooth_fft_cache()
{
    static thread_local SmoothFftCache* cache = new SmoothFftCache();
    return *cache;
}

// Converts one OFDM symbol with CP into centered frequency bins.
std::vector<CF> fft_without_cp(const std::vector<CF>& symbol, size_t n_fft, size_t n_cp)
{
    std::vector<CF> freq(n_fft);
    if (symbol.size() < n_fft + n_cp) {
        return freq;
    }

    ForwardFftCache& cache = forward_fft_cache();
    cache.ensure(n_fft);

    for (size_t i = 0; i < n_fft; ++i) {
        cache.in[i][0] = symbol[n_cp + i].real();
        cache.in[i][1] = symbol[n_cp + i].imag();
    }

    fftw_execute(cache.plan);

    for (size_t i = 0; i < n_fft; ++i) {
        const size_t centered_idx = (i + n_fft / 2) % n_fft;
        freq[centered_idx] = CF(cache.out[i][0], cache.out[i][1]) / static_cast<float>(n_fft);
    }

    return freq;
}

// Spectrum mode accounts for mirror / conjugate ambiguities in the RX path.
std::vector<CF> apply_spectrum_mode(const std::vector<CF>& freq, int mode)
{
    std::vector<CF> out(freq.size());
    const size_t n = freq.size();

    for (size_t k = 0; k < n; ++k) {
        const size_t mirror = (n - k) % n;
        if (mode == 1) {
            out[k] = std::conj(freq[k]);
        } else if (mode == 2) {
            out[k] = freq[mirror];
        } else if (mode == 3) {
            out[k] = std::conj(freq[mirror]);
        } else {
            out[k] = freq[k];
        }
    }

    return out;
}

// Circular shift compensates subcarrier offset after synchronization.
std::vector<CF> apply_subcarrier_shift(const std::vector<CF>& freq, int shift)
{
    if (freq.empty() || shift == 0) {
        return freq;
    }

    std::vector<CF> out(freq.size());
    const int n = static_cast<int>(freq.size());

    for (int k = 0; k < n; ++k) {
        int src = (k + shift) % n;
        if (src < 0) {
            src += n;
        }
        out[static_cast<size_t>(k)] = freq[static_cast<size_t>(src)];
    }

    return out;
}

std::vector<CF> transform_symbol_to_frequency(
    const std::vector<CF>& symbol,
    size_t n_fft,
    size_t n_cp,
    int spectrum_mode,
    int subcarrier_shift)
{
    return apply_subcarrier_shift(
        apply_spectrum_mode(fft_without_cp(symbol, n_fft, n_cp), spectrum_mode),
        subcarrier_shift
    );
}

std::vector<size_t> ordered_payload_indices(int data_order_mode)
{
    std::vector<size_t> indices = payload_data_indices();
    if (data_order_mode == 1) {
        std::reverse(indices.begin(), indices.end());
    }
    return indices;
}

std::vector<CF> expected_training_freq(size_t n_fft)
{
    std::vector<CF> expected(n_fft, CF(0.0f, 0.0f));
    const std::vector<size_t>& pilots = pilot_subcarrier_indices();
    const std::vector<size_t>& data_indices = payload_data_indices();
    const std::vector<CF> training = make_training_symbols(data_indices.size());

    for (size_t idx : pilots) {
        expected[idx] = KNOWN_PILOT;
    }
    for (size_t i = 0; i < data_indices.size(); ++i) {
        expected[data_indices[i]] = training[i];
    }

    return expected;
}

const std::vector<size_t>& active_training_indices()
{
    static const std::vector<size_t> active = [] {
        std::vector<size_t> indices = pilot_subcarrier_indices();
        const std::vector<size_t>& data = payload_data_indices();
        indices.insert(indices.end(), data.begin(), data.end());
        std::sort(indices.begin(), indices.end());
        return indices;
    }();

    return active;
}

// Base channel estimate from the known training OFDM symbol.
std::vector<CF> estimate_channel(
    const std::vector<CF>& training_freq,
    const std::vector<CF>& expected)
{
    std::vector<CF> channel(training_freq.size(), CF(1.0f, 0.0f));

    for (size_t idx : active_training_indices()) {
        if (idx >= training_freq.size() || idx >= expected.size()) {
            continue;
        }
        if (std::abs(expected[idx]) < 1e-6f) {
            continue;
        }

        const CF h = training_freq[idx] / expected[idx];
        if (std::abs(h) > 1e-6f) {
            channel[idx] = h;
        }
    }

    return channel;
}

std::vector<size_t> active_bins_without_dc()
{
    std::vector<size_t> bins;
    for (size_t idx = 28; idx <= 100; ++idx) {
        if (idx != 64) {
            bins.push_back(idx);
        }
    }
    return bins;
}

std::vector<CF> fill_channel_for_dft_smoothing(const std::vector<CF>& centered_channel)
{
    std::vector<CF> filled(LTE, CF(0.0f, 0.0f));
    if (centered_channel.size() < LTE) {
        return filled;
    }

    for (size_t idx = 28; idx <= 100; ++idx) {
        if (idx != 64) {
            filled[idx] = centered_channel[idx];
        }
    }

    for (size_t idx = 0; idx < 28; ++idx) {
        filled[idx] = filled[28];
    }
    for (size_t idx = 101; idx < LTE; ++idx) {
        filled[idx] = filled[100];
    }
    filled[64] = 0.5f * (filled[63] + filled[65]);

    return filled;
}

// DFT smoothing truncates the channel impulse response to CP length.
std::vector<CF> dft_smooth_channel(const std::vector<CF>& centered_channel)
{
    if (centered_channel.size() < LTE) {
        return {};
    }

    SmoothFftCache& cache = smooth_fft_cache();
    cache.ensure(LTE);
    const std::vector<CF> filled_channel = fill_channel_for_dft_smoothing(centered_channel);

    for (size_t i = 0; i < LTE; ++i) {
        const size_t centered_idx = (i + (LTE / 2)) % LTE;
        const CF value = filled_channel[centered_idx];
        cache.freq_in[i][0] = value.real();
        cache.freq_in[i][1] = value.imag();
    }

    fftw_execute(cache.ifft_plan);

    for (size_t i = 0; i < LTE; ++i) {
        CF tap(cache.time_out[i][0], cache.time_out[i][1]);
        tap /= static_cast<float>(LTE);
        if (i >= CP_LENGTH) {
            tap = CF(0.0f, 0.0f);
        }
        cache.time_in[i][0] = tap.real();
        cache.time_in[i][1] = tap.imag();
    }

    fftw_execute(cache.fft_plan);

    std::vector<CF> smoothed(LTE, CF(0.0f, 0.0f));
    for (size_t i = 0; i < LTE; ++i) {
        const size_t centered_idx = (i + (LTE / 2)) % LTE;
        smoothed[centered_idx] = CF(cache.freq_out[i][0], cache.freq_out[i][1]);
    }

    return smoothed;
}

ComplexMatrix make_identity(size_t n)
{
    ComplexMatrix identity(n, std::vector<CF>(n, CF(0.0f, 0.0f)));
    for (size_t i = 0; i < n; ++i) {
        identity[i][i] = CF(1.0f, 0.0f);
    }
    return identity;
}

bool invert_matrix(ComplexMatrix matrix, ComplexMatrix& inverse)
{
    const size_t n = matrix.size();
    inverse = make_identity(n);

    for (size_t col = 0; col < n; ++col) {
        size_t pivot = col;
        float pivot_abs = std::abs(matrix[pivot][col]);
        for (size_t row = col + 1; row < n; ++row) {
            const float current_abs = std::abs(matrix[row][col]);
            if (current_abs > pivot_abs) {
                pivot = row;
                pivot_abs = current_abs;
            }
        }

        if (pivot_abs < 1e-6f) {
            return false;
        }

        if (pivot != col) {
            std::swap(matrix[pivot], matrix[col]);
            std::swap(inverse[pivot], inverse[col]);
        }

        const CF diag = matrix[col][col];
        for (size_t j = 0; j < n; ++j) {
            matrix[col][j] /= diag;
            inverse[col][j] /= diag;
        }

        for (size_t row = 0; row < n; ++row) {
            if (row == col) {
                continue;
            }

            const CF factor = matrix[row][col];
            if (std::abs(factor) < 1e-9f) {
                continue;
            }

            for (size_t j = 0; j < n; ++j) {
                matrix[row][j] -= factor * matrix[col][j];
                inverse[row][j] -= factor * inverse[col][j];
            }
        }
    }

    return true;
}

CF correlation_entry(size_t a, size_t b)
{
    const float delta = static_cast<float>(std::abs(static_cast<int>(a) - static_cast<int>(b)));
    return CF(std::exp(-delta / kLmmseCorrDecay), 0.0f);
}

// Pilot-only LMMSE estimate used as a stronger alternative to plain pilot LS.
std::vector<CF> estimate_channel_with_pilot_lmmse(
    const std::vector<CF>& payload_freq,
    const std::vector<CF>& reference_channel)
{
    const std::vector<size_t>& pilots = pilot_subcarrier_indices();
    const std::vector<size_t> active_bins = active_bins_without_dc();
    std::vector<CF> output(payload_freq.size(), CF(0.0f, 0.0f));
    if (payload_freq.empty() || pilots.empty()) {
        return output;
    }

    std::vector<CF> pilot_ls;
    pilot_ls.reserve(pilots.size());
    for (size_t idx : pilots) {
        pilot_ls.push_back(payload_freq[idx] / KNOWN_PILOT);
    }

    float noise_var = 1e-3f;
    if (!reference_channel.empty()) {
        double error_power = 0.0;
        size_t count = 0;
        for (size_t idx : pilots) {
            if (idx < reference_channel.size() && std::abs(reference_channel[idx]) > 1e-6f) {
                error_power += std::norm(pilot_ls[count] - reference_channel[idx]);
            }
            ++count;
        }
        noise_var = static_cast<float>(std::max(error_power / std::max<size_t>(1, pilots.size()), 1e-4));
    }

    ComplexMatrix r_pp(pilots.size(), std::vector<CF>(pilots.size(), CF(0.0f, 0.0f)));
    for (size_t i = 0; i < pilots.size(); ++i) {
        for (size_t j = 0; j < pilots.size(); ++j) {
            r_pp[i][j] = correlation_entry(pilots[i], pilots[j]);
        }
        r_pp[i][i] += CF(noise_var, 0.0f);
    }

    ComplexMatrix r_pp_inv;
    if (!invert_matrix(r_pp, r_pp_inv)) {
        return estimate_channel_with_pilots_linear(payload_freq, pilots, KNOWN_PILOT);
    }

    std::vector<CF> weights(pilots.size(), CF(0.0f, 0.0f));
    for (size_t i = 0; i < pilots.size(); ++i) {
        for (size_t j = 0; j < pilots.size(); ++j) {
            weights[i] += r_pp_inv[i][j] * pilot_ls[j];
        }
    }

    for (size_t idx : active_bins) {
        CF h(0.0f, 0.0f);
        for (size_t p = 0; p < pilots.size(); ++p) {
            h += correlation_entry(idx, pilots[p]) * weights[p];
        }
        output[idx] = h;
    }

    for (size_t i = 0; i < pilots.size(); ++i) {
        output[pilots[i]] = pilot_ls[i];
    }

    return output;
}

// Residual pilot-based phase/gain correction after one-tap equalization.
CF pilot_alignment_fix(const std::vector<CF>& equalized_symbol)
{
    CF numerator(0.0f, 0.0f);
    double denominator = 0.0;
    for (size_t idx : pilot_subcarrier_indices()) {
        if (idx >= equalized_symbol.size()) {
            continue;
        }

        const CF& pilot = equalized_symbol[idx];
        if (std::abs(pilot) < 1e-6f) {
            continue;
        }

        numerator += std::conj(pilot) * KNOWN_PILOT;
        denominator += std::norm(pilot);
    }

    if (denominator < 1e-9 || std::abs(numerator) < 1e-6f) {
        return CF(1.0f, 0.0f);
    }

    return numerator / static_cast<float>(denominator);
}

std::vector<CF> apply_pilot_alignment(const std::vector<CF>& equalized_symbol)
{
    const CF correction = pilot_alignment_fix(equalized_symbol);
    std::vector<CF> fixed = equalized_symbol;
    for (CF& sample : fixed) {
        sample *= correction;
    }
    return fixed;
}

std::vector<CF> build_decision_directed_channel(
    const std::vector<CF>& payload_freq,
    const ModulationSpec& modulation,
    const std::vector<CF>& reference_channel)
{
    std::vector<CF> channel = estimate_channel_with_pilot_lmmse(payload_freq, reference_channel);
    std::vector<CF> equalized = apply_pilot_alignment(equalize_with_channel(payload_freq, channel));

    for (size_t idx : payload_data_indices()) {
        if (idx >= payload_freq.size() || idx >= equalized.size()) {
            continue;
        }
        const CF hard_symbol = hard_decision_symbol(equalized[idx], modulation);
        if (std::abs(hard_symbol) > 1e-6f) {
            channel[idx] = payload_freq[idx] / hard_symbol;
        }
    }

    for (size_t idx : pilot_subcarrier_indices()) {
        if (idx < payload_freq.size()) {
            channel[idx] = payload_freq[idx] / KNOWN_PILOT;
        }
    }

    return dft_smooth_channel(channel);
}

// Selects the estimator used for payload equalization in the full decoder.
std::vector<CF> channel_for_method(
    ChannelEstimatorMethod estimator_method,
    const std::vector<CF>& payload_freq,
    const std::vector<CF>& training_channel_ls,
    const std::vector<CF>& training_channel_dft,
    const ModulationSpec& modulation)
{
    switch (estimator_method) {
        case ChannelEstimatorMethod::TrainingLS:
            return training_channel_ls;
        case ChannelEstimatorMethod::PilotLSLinear:
            return estimate_channel_with_pilots_linear(
                payload_freq,
                pilot_subcarrier_indices(),
                KNOWN_PILOT
            );
        case ChannelEstimatorMethod::PilotLMMSE:
            return estimate_channel_with_pilot_lmmse(payload_freq, training_channel_dft);
        case ChannelEstimatorMethod::DFTLS:
            return training_channel_dft;
        case ChannelEstimatorMethod::DecisionDirected:
            return build_decision_directed_channel(payload_freq, modulation, training_channel_dft);
    }

    return training_channel_ls;
}

// Validates magic + CRC and extracts recovered text from decoded bytes.
void unpack_decoded_bytes(DecodedResult& result)
{
    if (result.raw_bytes.size() < 5) {
        return;
    }

    if (result.raw_bytes[0] != kMagic0 || result.raw_bytes[1] != kMagic1) {
        return;
    }

    result.payload_length = static_cast<size_t>(result.raw_bytes[2]);
    if (result.payload_length + 5 > result.raw_bytes.size()) {
        return;
    }

    std::vector<uint8_t> crc_input(
        result.raw_bytes.begin(),
        result.raw_bytes.begin() + static_cast<ptrdiff_t>(3 + result.payload_length)
    );
    const uint16_t expected_crc = crc16_ccitt(crc_input);
    const uint16_t rx_crc =
        (static_cast<uint16_t>(result.raw_bytes[3 + result.payload_length]) << 8) |
        static_cast<uint16_t>(result.raw_bytes[4 + result.payload_length]);

    result.crc_ok = (expected_crc == rx_crc);
    result.recovered_text.assign(
        result.raw_bytes.begin() + 3,
        result.raw_bytes.begin() + static_cast<ptrdiff_t>(3 + result.payload_length)
    );
}

}  // namespace

const std::vector<ChannelEstimatorMethod>& channel_estimator_methods()
{
    static const std::vector<ChannelEstimatorMethod> methods = {
        ChannelEstimatorMethod::TrainingLS,
        ChannelEstimatorMethod::PilotLSLinear,
        ChannelEstimatorMethod::PilotLMMSE,
        ChannelEstimatorMethod::DFTLS,
        ChannelEstimatorMethod::DecisionDirected
    };
    return methods;
}

const char* channel_estimator_name(ChannelEstimatorMethod method)
{
    switch (method) {
        case ChannelEstimatorMethod::TrainingLS:
            return "Training LS";
        case ChannelEstimatorMethod::PilotLSLinear:
            return "Pilot LS Linear";
        case ChannelEstimatorMethod::PilotLMMSE:
            return "Pilot LMMSE";
        case ChannelEstimatorMethod::DFTLS:
            return "DFT LS";
        case ChannelEstimatorMethod::DecisionDirected:
            return "Decision Directed";
    }

    return "Unknown";
}

// Lightweight live estimate used by the real-time RX monitor.
std::vector<CF> estimate_live_channel(
    const std::vector<CF>& payload_freq,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method)
{
    const std::vector<size_t>& pilots = pilot_subcarrier_indices();
    const std::vector<CF> pilot_ls =
        estimate_channel_with_pilots_linear(payload_freq, pilots, KNOWN_PILOT);
    const std::vector<CF> pilot_dft = dft_smooth_channel(pilot_ls);

    switch (estimator_method) {
        case ChannelEstimatorMethod::TrainingLS:
            return pilot_ls;
        case ChannelEstimatorMethod::PilotLSLinear:
            return pilot_ls;
        case ChannelEstimatorMethod::PilotLMMSE:
            return estimate_channel_with_pilot_lmmse(payload_freq, pilot_dft);
        case ChannelEstimatorMethod::DFTLS:
            return pilot_dft;
        case ChannelEstimatorMethod::DecisionDirected:
            return build_decision_directed_channel(payload_freq, modulation, pilot_dft);
    }

    return pilot_ls;
}

std::vector<CF> equalize_live_symbol(
    const std::vector<CF>& payload_freq,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method,
    std::vector<CF>* channel_estimate)
{
    std::vector<CF> channel = estimate_live_channel(payload_freq, modulation, estimator_method);
    std::vector<CF> equalized = apply_pilot_alignment(equalize_with_channel(payload_freq, channel));
    if (channel_estimate != nullptr) {
        *channel_estimate = std::move(channel);
    }
    return equalized;
}

// Full payload decoder that starts from the known training symbol.
DecodedResult decode_ofdm_stream_with_training(
    const std::vector<CF>& training_symbol,
    const std::vector<CF>& data_fixed,
    const ModulationSpec& modulation,
    size_t n_fft,
    size_t n_cp,
    size_t used_data_subcarriers,
    ChannelEstimatorMethod estimator_method,
    int spectrum_mode,
    int data_order_mode,
    int subcarrier_shift)
{
    DecodedResult result;
    const size_t symbol_len = n_fft + n_cp;
    if (training_symbol.size() < symbol_len || data_fixed.size() < symbol_len) {
        return result;
    }

    const std::vector<CF> expected = expected_training_freq(n_fft);
    const std::vector<CF> training_freq = transform_symbol_to_frequency(
        training_symbol,
        n_fft,
        n_cp,
        spectrum_mode,
        subcarrier_shift
    );
    const std::vector<CF> training_channel_ls = estimate_channel(training_freq, expected);
    const std::vector<CF> training_channel_dft = dft_smooth_channel(training_channel_ls);

    std::vector<size_t> data_indices = ordered_payload_indices(data_order_mode);
    if (used_data_subcarriers > 0 && used_data_subcarriers < data_indices.size()) {
        data_indices.resize(used_data_subcarriers);
    }

    std::vector<int8_t> all_bits;
    const size_t total_symbols = data_fixed.size() / symbol_len;

    for (size_t s = 0; s < total_symbols; ++s) {
        std::vector<CF> symbol(
            data_fixed.begin() + static_cast<ptrdiff_t>(s * symbol_len),
            data_fixed.begin() + static_cast<ptrdiff_t>((s + 1) * symbol_len)
        );
        const std::vector<CF> freq = transform_symbol_to_frequency(
            symbol,
            n_fft,
            n_cp,
            spectrum_mode,
            subcarrier_shift
        );

        const std::vector<CF> channel = channel_for_method(
            estimator_method,
            freq,
            training_channel_ls,
            training_channel_dft,
            modulation
        );
        const std::vector<CF> equalized = apply_pilot_alignment(equalize_with_channel(freq, channel));

        std::vector<CF> points;
        points.reserve(data_indices.size());
        for (size_t idx : data_indices) {
            points.push_back(equalized[idx]);
        }

        if (s == 0) {
            result.constellation_points = points;
            result.channel_estimate = channel;
            result.equalized_symbol = equalized;
        }

        std::vector<int8_t> bits;
        bits.reserve(points.size() * modulation.bits_per_symbol);
        for (const CF& point : points) {
            const std::vector<int8_t> symbol_bits = demodulate_symbol(point, modulation);
            bits.insert(bits.end(), symbol_bits.begin(), symbol_bits.end());
        }
        all_bits.insert(all_bits.end(), bits.begin(), bits.end());
    }

    result.raw_bytes = bits_to_bytes(all_bits);
    result.symbols_processed = total_symbols;
    unpack_decoded_bytes(result);
    return result;
}

// The functions below are scoring helpers used by frame search in ofdm_radio.cpp.
double estimate_training_channel_roughness(
    const std::vector<CF>& training_symbol,
    size_t n_fft,
    size_t n_cp,
    int spectrum_mode,
    int subcarrier_shift)
{
    if (training_symbol.size() < n_fft + n_cp) {
        return 1000.0;
    }

    const std::vector<CF> expected = expected_training_freq(n_fft);
    const std::vector<CF> freq = transform_symbol_to_frequency(
        training_symbol,
        n_fft,
        n_cp,
        spectrum_mode,
        subcarrier_shift
    );

    std::vector<CF> channel_values;
    for (size_t idx : active_training_indices()) {
        if (idx < freq.size() && idx < expected.size() && std::abs(expected[idx]) > 1e-6f) {
            channel_values.push_back(freq[idx] / expected[idx]);
        }
    }

    if (channel_values.size() < 2) {
        return 1000.0;
    }

    double avg_power = 0.0;
    for (const CF& h : channel_values) {
        avg_power += std::norm(h);
    }
    avg_power = std::max(avg_power / static_cast<double>(channel_values.size()), 1e-9);

    double roughness = 0.0;
    for (size_t i = 1; i < channel_values.size(); ++i) {
        roughness += std::norm(channel_values[i] - channel_values[i - 1]);
    }

    return (roughness / static_cast<double>(channel_values.size() - 1)) / avg_power;
}

double estimate_training_match_score(
    const std::vector<CF>& training_symbol,
    size_t n_fft,
    size_t n_cp,
    int spectrum_mode,
    int subcarrier_shift)
{
    if (training_symbol.size() < n_fft + n_cp) {
        return 0.0;
    }

    const std::vector<CF> expected = expected_training_freq(n_fft);
    const std::vector<CF> freq = transform_symbol_to_frequency(
        training_symbol,
        n_fft,
        n_cp,
        spectrum_mode,
        subcarrier_shift
    );

    CF corr(0.0f, 0.0f);
    double rx_energy = 0.0;
    double ref_energy = 0.0;

    for (size_t idx : active_training_indices()) {
        corr += freq[idx] * std::conj(expected[idx]);
        rx_energy += std::norm(freq[idx]);
        ref_energy += std::norm(expected[idx]);
    }

    if (rx_energy <= 0.0 || ref_energy <= 0.0) {
        return 0.0;
    }

    return std::abs(corr) / std::sqrt(rx_energy * ref_energy);
}

double estimate_training_decode_score(
    const std::vector<CF>& training_symbol,
    size_t n_fft,
    size_t n_cp,
    size_t used_data_subcarriers,
    int spectrum_mode,
    int data_order_mode,
    int subcarrier_shift)
{
    if (training_symbol.size() < n_fft + n_cp) {
        return 0.0;
    }

    std::vector<size_t> data_indices = ordered_payload_indices(data_order_mode);
    std::vector<CF> training_data = make_training_symbols(payload_data_indices().size());
    if (data_order_mode == 1) {
        std::reverse(training_data.begin(), training_data.end());
    }
    if (used_data_subcarriers > 0 && used_data_subcarriers < data_indices.size()) {
        data_indices.resize(used_data_subcarriers);
        training_data.resize(used_data_subcarriers);
    }

    const std::vector<CF> freq = transform_symbol_to_frequency(
        training_symbol,
        n_fft,
        n_cp,
        spectrum_mode,
        subcarrier_shift
    );
    const std::vector<CF> equalized =
        equalize_with_pilots(freq, pilot_subcarrier_indices(), KNOWN_PILOT);

    double mse = 0.0;
    for (size_t i = 0; i < data_indices.size(); ++i) {
        mse += std::norm(equalized[data_indices[i]] - training_data[i]);
    }

    if (data_indices.empty()) {
        return 0.0;
    }

    mse /= static_cast<double>(data_indices.size());
    return 1.0 / std::max(mse, 1e-9);
}
