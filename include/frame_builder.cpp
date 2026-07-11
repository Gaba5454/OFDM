#include "frame_builder.h"

#include "cycle_prefix.h"
#include "frame_layout.h"
#include "ofdm_symbol.h"
#include "payload_codec.h"
#include "pss_generator.h"

TxFrameData build_tx_frame(const std::string& text, const ModulationSpec& modulation)
{
    TxFrameData frame;
    const std::string clamped_text = text.substr(0, MAX_TEXT_BYTES);
    const size_t data_count = payload_data_indices().size();

    frame.max_text_bytes = MAX_TEXT_BYTES;
    frame.text_was_truncated = text.size() > MAX_TEXT_BYTES;
    frame.payload_symbol_count = payload_symbol_count_for_text(clamped_text.size(), modulation);

    const size_t payload_bytes_capacity = frame.payload_symbol_count * payload_bytes_per_symbol(modulation);
    const std::vector<uint8_t> payload_bytes = pack_payload_bytes(clamped_text, payload_bytes_capacity);

    frame.bits = bytes_to_bits(payload_bytes);
    frame.modulated_symbols = modulate_bits(frame.bits, modulation);

    const std::vector<CF> pss = primary_synchronization_signal(1);
    frame.pss_with_cp = cyclicPrefix(pss, CP_LENGTH);

    const std::vector<CF> training = make_training_symbols(data_count);
    frame.training_ofdm = ofdm(training);
    frame.training_with_cp = cyclicPrefix(frame.training_ofdm, CP_LENGTH);

    frame.samples.reserve((2 + frame.payload_symbol_count) * SYMBOL_LEN);
    frame.samples.insert(frame.samples.end(), frame.pss_with_cp.begin(), frame.pss_with_cp.end());
    frame.samples.insert(frame.samples.end(), frame.training_with_cp.begin(), frame.training_with_cp.end());

    for (size_t sym = 0; sym < frame.payload_symbol_count; ++sym) {
        const size_t begin_idx = sym * data_count;
        std::vector<CF> payload_symbol(
            frame.modulated_symbols.begin() + static_cast<ptrdiff_t>(begin_idx),
            frame.modulated_symbols.begin() + static_cast<ptrdiff_t>(begin_idx + data_count)
        );
        const std::vector<CF> payload_with_cp = cyclicPrefix(ofdm(payload_symbol), CP_LENGTH);
        frame.samples.insert(frame.samples.end(), payload_with_cp.begin(), payload_with_cp.end());
    }

    return frame;
}
