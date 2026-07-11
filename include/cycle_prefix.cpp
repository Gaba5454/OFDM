#include "cycle_prefix.h"

#include <stdexcept>

std::vector<CF> cyclicPrefix(const std::vector<CF>& ofdm_symbol, size_t cp_len)
{
    if (cp_len > ofdm_symbol.size()) {
        throw std::invalid_argument("cyclic prefix length is larger than OFDM symbol length");
    }

    if (cp_len == 0) {
        return ofdm_symbol;
    }

    std::vector<CF> output;
    output.reserve(ofdm_symbol.size() + cp_len);

    output.insert(output.end(),
                  ofdm_symbol.end() - static_cast<ptrdiff_t>(cp_len),
                  ofdm_symbol.end());

    output.insert(output.end(),
                  ofdm_symbol.begin(),
                  ofdm_symbol.end());

    return output;
}
