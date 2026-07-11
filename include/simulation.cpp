#include "simulation.h"

#include "cfo_functions.h"
#include "channel_simulate.h"
#include "corellations.h"
#include "frame_builder.h"
#include "frame_layout.h"
#include "training_decoder.h"

namespace {

constexpr const char* kDefaultSimulationText = "BUREAU1440";
constexpr size_t kSimulationFrameRepeats = 10;

}  // namespace

GuiPlotData build_simulation_view(
    const std::string& text,
    double snr,
    const ModulationSpec& modulation)
{
    GuiPlotData view;
    view.modulation_name = modulation.name;
    view.snr = snr;
    view.max_text_bytes = MAX_TEXT_BYTES;
    view.text_was_truncated = text.size() > MAX_TEXT_BYTES;
    view.original_text = text.substr(0, MAX_TEXT_BYTES);

    const TxFrameData frame = build_tx_frame(view.original_text, modulation);

    view.raw_bits = frame.bits;
    view.modulated_symbols = frame.modulated_symbols;
    view.ideal_constellation = ideal_constellation_points(modulation);

    view.pss_signal = frame.pss_with_cp;
    view.ofdm_symbols = frame.training_ofdm;
    view.ofdm_with_cp = frame.training_with_cp;

    view.tx_array.reserve(kSimulationFrameRepeats * frame.samples.size());
    for (size_t i = 0; i < kSimulationFrameRepeats; ++i) {
        view.tx_array.insert(view.tx_array.end(), frame.samples.begin(), frame.samples.end());
    }

    const std::vector<CF> rx_array = channelSimulation(view.tx_array, snr);
    view.correlation_map = correlationPSS(rx_array, view.pss_signal);
    view.peak_position = findCorrelationPeak(view.correlation_map);

    if (view.peak_position == SIZE_MAX) {
        return view;
    }

    const size_t training_start = view.peak_position + SYMBOL_LEN;
    if (training_start + SYMBOL_LEN * (frame.payload_symbol_count + 1) > rx_array.size()) {
        return view;
    }

    view.data_after_pss.assign(
        rx_array.begin() + static_cast<ptrdiff_t>(training_start),
        rx_array.begin() + static_cast<ptrdiff_t>(training_start + SYMBOL_LEN * (frame.payload_symbol_count + 1))
    );

    const std::vector<CF> first_symbol(
        view.data_after_pss.begin(),
        view.data_after_pss.begin() + static_cast<ptrdiff_t>(SYMBOL_LEN)
    );
    const double cfo = estimate_cfo(first_symbol, LTE, CP_LENGTH, 1e-3);
    const std::vector<CF> data_fixed = compensate_cfo(view.data_after_pss, cfo);

    const std::vector<CF> training_fixed(
        data_fixed.begin(),
        data_fixed.begin() + static_cast<ptrdiff_t>(SYMBOL_LEN)
    );
    const std::vector<CF> payload_fixed(
        data_fixed.begin() + static_cast<ptrdiff_t>(SYMBOL_LEN),
        data_fixed.end()
    );

    const DecodedResult decoded = decode_ofdm_stream_with_training(
        training_fixed,
        payload_fixed,
        modulation,
        LTE,
        CP_LENGTH,
        payload_data_indices().size()
    );

    view.recovered_text = decoded.recovered_text.substr(0, view.original_text.size());
    view.received_constellation = decoded.constellation_points;
    view.crc_ok = decoded.crc_ok;
    return view;
}

void simulation(double snr, const ModulationSpec& modulation)
{
    run_simulation_gui(kDefaultSimulationText, snr, modulation.name);
}
