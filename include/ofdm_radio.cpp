#include "ofdm_radio.h"

#include "cfo_functions.h"
#include "equalizer.h"
#include "fftw_guard.h"
#include "frame_layout.h"
#include "corellations.h"
#include "pss_generator.h"
#include "training_decoder.h"

#include <SoapySDR/Device.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <vector>
#include <fftw3.h>

SoapySDRDevice* open_pluto(const char* device_uri, std::string* error_message)
{
    static std::mutex open_mutex;
    std::lock_guard<std::mutex> lock(open_mutex);

    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", device_uri);
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "loopback", "0");

    SoapySDRDevice* sdr = SoapySDRDevice_make(&args);
    if (sdr == nullptr && error_message != nullptr) {
        *error_message = SoapySDRDevice_lastError();
    }
    SoapySDRKwargs_clear(&args);
    return sdr;
}

std::vector<CF> slice_samples(const std::vector<CF>& samples, size_t start, size_t len)
{
    if (start > samples.size() || len > samples.size() - start) {
        return {};
    }

    return {
        samples.begin() + static_cast<ptrdiff_t>(start),
        samples.begin() + static_cast<ptrdiff_t>(start + len)
    };
}

double cp_metric(const std::vector<CF>& samples, size_t start)
{
    if (start > samples.size() || SYMBOL_LEN > samples.size() - start) {
        return 0.0;
    }

    CF sum(0.0f, 0.0f);
    double cp_energy = 0.0;
    double tail_energy = 0.0;

    for (size_t i = 0; i < CP_LENGTH; ++i) {
        const CF& cp = samples[start + i];
        const CF& tail = samples[start + LTE + i];
        sum += cp * std::conj(tail);
        cp_energy += std::norm(cp);
        tail_energy += std::norm(tail);
    }

    if (cp_energy <= 0.0 || tail_energy <= 0.0) {
        return 0.0;
    }

    return std::abs(sum) / std::sqrt(cp_energy * tail_energy);
}

std::vector<double> correlation_pss_cfo_search(
    const std::vector<CF>& rx_samples,
    const std::vector<CF>& pss,
    std::vector<double>& best_cfo_hz)
{
    constexpr double min_cfo_hz = -30000.0;
    constexpr double max_cfo_hz = 30000.0;
    constexpr double step_hz = 1000.0;

    if (rx_samples.size() < pss.size() || pss.empty()) {
        best_cfo_hz.clear();
        return {};
    }

    const size_t corr_size = rx_samples.size() - pss.size() + 1;
    std::vector<double> best_corr(corr_size, 0.0);
    best_cfo_hz.assign(corr_size, 0.0);

    double pss_energy = 0.0;
    for (const CF& sample : pss) {
        pss_energy += std::norm(sample);
    }

    for (double cfo_hz = min_cfo_hz; cfo_hz <= max_cfo_hz; cfo_hz += step_hz) {
        std::vector<CF> shifted_pss(pss.size());
        const double phase_step = 2.0 * M_PI * cfo_hz / SDR_SAMPLE_RATE;
        double phase = 0.0;

        for (size_t i = 0; i < pss.size(); ++i) {
            shifted_pss[i] = pss[i] * std::exp(CF(0.0f, static_cast<float>(phase)));
            phase += phase_step;
        }

        for (size_t k = 0; k < corr_size; ++k) {
            CF sum(0.0f, 0.0f);
            double rx_energy = 0.0;

            for (size_t i = 0; i < pss.size(); ++i) {
                const CF& rx = rx_samples[k + i];
                sum += rx * std::conj(shifted_pss[i]);
                rx_energy += std::norm(rx);
            }

            if (rx_energy <= 0.0 || pss_energy <= 0.0) {
                continue;
            }

            const double corr = std::abs(sum) / std::sqrt(pss_energy * rx_energy);
            if (corr > best_corr[k]) {
                best_corr[k] = corr;
                best_cfo_hz[k] = cfo_hz;
            }
        }
    }

    return best_corr;
}

namespace {

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

ForwardFftCache& monitor_fft_cache()
{
    static thread_local ForwardFftCache* cache = new ForwardFftCache();
    return *cache;
}

std::vector<CF> monitor_fft_without_cp(const std::vector<CF>& symbol)
{
    std::vector<CF> freq(LTE, CF(0.0f, 0.0f));
    if (symbol.size() < SYMBOL_LEN) {
        return freq;
    }

    ForwardFftCache& cache = monitor_fft_cache();
    cache.ensure(LTE);

    for (size_t i = 0; i < LTE; ++i) {
        cache.in[i][0] = symbol[CP_LENGTH + i].real();
        cache.in[i][1] = symbol[CP_LENGTH + i].imag();
    }

    fftw_execute(cache.plan);

    for (size_t i = 0; i < LTE; ++i) {
        const size_t centered_idx = (i + LTE / 2) % LTE;
        freq[centered_idx] = CF(cache.out[i][0], cache.out[i][1]) / static_cast<float>(LTE);
    }

    return freq;
}

std::vector<CF> monitor_apply_spectrum_mode(const std::vector<CF>& freq, int mode)
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

std::vector<CF> monitor_apply_subcarrier_shift(const std::vector<CF>& freq, int shift)
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

double pilot_match_score(const std::vector<CF>& equalized)
{
    const std::vector<size_t>& pilots = pilot_subcarrier_indices();
    if (equalized.empty() || pilots.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    double score = 0.0;
    size_t count = 0;
    for (size_t idx : pilots) {
        if (idx >= equalized.size()) {
            continue;
        }
        score += std::abs(equalized[idx] - KNOWN_PILOT);
        ++count;
    }

    if (count == 0) {
        return std::numeric_limits<double>::infinity();
    }
    return score / static_cast<double>(count);
}

SyncInfo find_frame_start(
    const std::vector<CF>& rx_samples,
    const std::vector<double>& corr_map,
    const std::vector<double>& cfo_map,
    size_t required_payload_symbols)
{
    SyncInfo sync;
    const CorrelationStats stats = analyzeCorrelationMap(corr_map);
    const double floor = std::max(
        0.20,
        std::max(stats.mean_value + 2.5 * stats.stddev_value, stats.max_value * 0.75)
    );

    for (size_t i = 1; i + 1 < corr_map.size(); ++i) {
        if (corr_map[i] < floor || corr_map[i] < corr_map[i - 1] || corr_map[i] < corr_map[i + 1]) {
            continue;
        }
        if (i < CP_LENGTH) {
            continue;
        }

        const size_t pss_start = i - CP_LENGTH;
        const size_t training_start = pss_start + SYMBOL_LEN;
        const size_t payload_start = training_start + SYMBOL_LEN;
        if (payload_start + required_payload_symbols * SYMBOL_LEN > rx_samples.size()) {
            continue;
        }

        ++sync.candidates;
        const double score = corr_map[i]
            + 0.05 * cp_metric(rx_samples, pss_start)
            + 0.05 * cp_metric(rx_samples, training_start)
            + 0.05 * cp_metric(rx_samples, payload_start);
        const double best_score = sync.pss_corr
            + 0.05 * cp_metric(rx_samples, sync.pss_start)
            + 0.05 * cp_metric(rx_samples, sync.training_start)
            + 0.05 * cp_metric(rx_samples, sync.payload_start);

        if (corr_map[i] > sync.pss_corr + 1e-6 ||
            (std::abs(corr_map[i] - sync.pss_corr) <= 1e-6 && score > best_score)) {
            sync.pss_peak = i;
            sync.pss_start = pss_start;
            sync.training_start = training_start;
            sync.payload_start = payload_start;
            sync.pss_corr = corr_map[i];
            sync.cfo_hz = (i < cfo_map.size()) ? cfo_map[i] : 0.0;
        }
    }

    return sync;
}

DecodedResult decode_payload(
    const std::vector<CF>& training,
    const std::vector<CF>& payload,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method,
    int& spectrum_mode,
    int& subcarrier_shift,
    int& data_order,
    const DecodeSearchHints* search_hints)
{
    DecodedResult best;
    double best_score = -std::numeric_limits<double>::infinity();
    const size_t data_count = payload_data_indices().size();

    auto search_range = [&](int mode_min, int mode_max, int shift_min, int shift_max) {
        for (int mode = mode_min; mode <= mode_max; ++mode) {
            for (int shift = shift_min; shift <= shift_max; ++shift) {
                const double match = estimate_training_match_score(training, LTE, CP_LENGTH, mode, shift);
                double roughness = estimate_training_channel_roughness(training, LTE, CP_LENGTH, mode, shift);
                if (!std::isfinite(roughness)) {
                    roughness = 1000.0;
                }

                for (int order = 0; order <= 1; ++order) {
                    const double training_score =
                        estimate_training_decode_score(training, LTE, CP_LENGTH, data_count, mode, order, shift);
                    DecodedResult decoded =
                        decode_ofdm_stream_with_training(
                            training,
                            payload,
                            modulation,
                            LTE,
                            CP_LENGTH,
                            data_count,
                            estimator_method,
                            mode,
                            order,
                            shift
                        );
                    const double mse = constellation_mse(decoded.constellation_points, modulation);
                    const double score =
                        (decoded.crc_ok ? 10000.0 : 0.0) +
                        20.0 * match +
                        10.0 * training_score -
                        0.25 * roughness -
                        mse;

                    if (score > best_score) {
                        best = std::move(decoded);
                        spectrum_mode = mode;
                        subcarrier_shift = shift;
                        data_order = order;
                        best_score = score;
                    }
                }
            }
        }
    };

    bool used_narrowed_search = false;
    if (search_hints != nullptr &&
        (search_hints->use_spectrum_mode || search_hints->use_subcarrier_shift)) {
        int mode_min = 0;
        int mode_max = 3;
        if (search_hints->use_spectrum_mode) {
            const int hinted_mode = std::clamp(search_hints->spectrum_mode, 0, 3);
            mode_min = hinted_mode;
            mode_max = hinted_mode;
        }

        int shift_min = -16;
        int shift_max = 16;
        if (search_hints->use_subcarrier_shift) {
            const int hinted_shift = std::clamp(search_hints->subcarrier_shift, -16, 16);
            shift_min = std::max(-16, hinted_shift - 2);
            shift_max = std::min(16, hinted_shift + 2);
        }

        search_range(mode_min, mode_max, shift_min, shift_max);
        used_narrowed_search = (mode_min != 0 || mode_max != 3 || shift_min != -16 || shift_max != 16);
    }

    if (!used_narrowed_search || !best.crc_ok) {
        search_range(0, 3, -16, 16);
    }

    return best;
}

}  // namespace

LiveMonitorInfo analyze_live_monitor(
    const std::vector<CF>& rx_samples,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method)
{
    LiveMonitorInfo info;
    if (rx_samples.size() < SYMBOL_LEN) {
        return info;
    }

    size_t best_start = SIZE_MAX;
    double best_cp = -1.0;
    for (size_t start = 0; start + SYMBOL_LEN <= rx_samples.size(); ++start) {
        const double score = cp_metric(rx_samples, start);
        if (score > best_cp) {
            best_cp = score;
            best_start = start;
        }
    }

    if (best_start == SIZE_MAX) {
        return info;
    }

    const std::vector<CF> symbol = slice_samples(rx_samples, best_start, SYMBOL_LEN);
    const std::vector<CF> base_freq = monitor_fft_without_cp(symbol);
    const std::vector<size_t>& data_indices = payload_data_indices();

    double best_score = std::numeric_limits<double>::infinity();
    std::vector<CF> best_channel;
    std::vector<CF> best_equalized;
    int best_mode = 0;
    int best_shift = 0;

    for (int mode = 0; mode <= 3; ++mode) {
        for (int shift = -8; shift <= 8; ++shift) {
            const std::vector<CF> freq =
                monitor_apply_subcarrier_shift(monitor_apply_spectrum_mode(base_freq, mode), shift);
            std::vector<CF> channel;
            const std::vector<CF> equalized =
                equalize_live_symbol(freq, modulation, estimator_method, &channel);
            const double score = pilot_match_score(equalized);
            if (score < best_score) {
                best_score = score;
                best_channel = channel;
                best_equalized = equalized;
                best_mode = mode;
                best_shift = shift;
            }
        }
    }

    if (best_equalized.empty()) {
        return info;
    }

    info.constellation_points.reserve(data_indices.size());
    for (size_t idx : data_indices) {
        if (idx < best_equalized.size()) {
            info.constellation_points.push_back(best_equalized[idx]);
        }
    }

    info.channel_estimate = std::move(best_channel);
    info.equalized_symbol = std::move(best_equalized);
    info.symbol_start = best_start;
    info.cp_score = best_cp;
    info.spectrum_mode = best_mode;
    info.subcarrier_shift = best_shift;
    info.available = !info.constellation_points.empty();
    return info;
}

CaptureInfo decode_capture(
    const std::vector<CF>& rx_samples,
    const ModulationSpec& modulation,
    ChannelEstimatorMethod estimator_method,
    const DecodeSearchHints* search_hints)
{
    CaptureInfo capture;
    if (rx_samples.empty()) {
        return capture;
    }

    const std::vector<CF> pss = primary_synchronization_signal(1);
    std::vector<double> cfo_map;
    const size_t max_payload_symbols = payload_symbol_count_for_text(MAX_TEXT_BYTES, modulation);
    capture.corr_map = correlation_pss_cfo_search(rx_samples, pss, cfo_map);
    capture.sync = find_frame_start(rx_samples, capture.corr_map, cfo_map, max_payload_symbols);
    if (capture.sync.pss_peak == SIZE_MAX) {
        capture.sync = find_frame_start(rx_samples, capture.corr_map, cfo_map, 1);
    }
    capture.frame_found = (capture.sync.pss_peak != SIZE_MAX);
    if (!capture.frame_found) {
        return capture;
    }

    if (capture.sync.payload_start >= rx_samples.size()) {
        return capture;
    }

    const size_t available_payload_symbols =
        (rx_samples.size() - capture.sync.payload_start) / SYMBOL_LEN;
    const size_t payload_symbols_to_try = std::min(max_payload_symbols, available_payload_symbols);
    if (payload_symbols_to_try == 0) {
        return capture;
    }

    capture.payload_ready = true;
    capture.coarse_cfo = -capture.sync.cfo_hz / SDR_SAMPLE_RATE;

    const size_t frame_symbols = 2 + payload_symbols_to_try;
    std::vector<CF> fixed_samples = compensate_cfo(rx_samples, capture.coarse_cfo);
    std::vector<CF> frame = slice_samples(
        fixed_samples,
        capture.sync.pss_start,
        frame_symbols * SYMBOL_LEN
    );
    if (frame.size() < frame_symbols * SYMBOL_LEN) {
        return capture;
    }

    const std::vector<CF> training_coarse = slice_samples(frame, SYMBOL_LEN, SYMBOL_LEN);
    capture.fine_cfo = estimate_cfo(training_coarse, LTE, CP_LENGTH, 1e-3);
    if (!std::isfinite(capture.fine_cfo)) {
        capture.fine_cfo = 0.0;
    }

    capture.cfo = capture.coarse_cfo + capture.fine_cfo;
    if (std::abs(capture.fine_cfo) > 1e-9) {
        fixed_samples = compensate_cfo(fixed_samples, capture.fine_cfo);
        frame = slice_samples(
            fixed_samples,
            capture.sync.pss_start,
            frame_symbols * SYMBOL_LEN
        );
        if (frame.size() < frame_symbols * SYMBOL_LEN) {
            return capture;
        }
    }

    const std::vector<CF> training_fixed = slice_samples(frame, SYMBOL_LEN, SYMBOL_LEN);

    const std::vector<CF> payload_fixed = slice_samples(
        frame,
        2 * SYMBOL_LEN,
        payload_symbols_to_try * SYMBOL_LEN
    );

    int selected_spectrum_mode = 0;
    int selected_subcarrier_shift = 0;
    int selected_data_order = 0;
    DecodedResult best_decoded = decode_payload(
        training_fixed,
        payload_fixed,
        modulation,
        estimator_method,
        selected_spectrum_mode,
        selected_subcarrier_shift,
        selected_data_order,
        search_hints
    );

    capture.decoded = std::move(best_decoded);
    capture.spectrum_mode = selected_spectrum_mode;
    capture.subcarrier_shift = selected_subcarrier_shift;
    capture.data_order = selected_data_order;
    capture.data_after_pss = slice_samples(
        frame,
        SYMBOL_LEN,
        (1 + payload_symbols_to_try) * SYMBOL_LEN
    );

    return capture;
}
