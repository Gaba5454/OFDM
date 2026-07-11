#include "payload_codec.h"

#include "const.h"

#include <algorithm>

namespace {

constexpr uint8_t kMagic0 = 0x4Fu;
constexpr uint8_t kMagic1 = 0x44u;

}  // namespace

std::vector<uint8_t> bits_to_bytes(const std::vector<int8_t>& bits)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(bits.size() / 8);

    for (size_t i = 0; i + 7 < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (size_t b = 0; b < 8; ++b) {
            byte |= static_cast<uint8_t>((bits[i + b] != 0 ? 1u : 0u) << (7 - b));
        }
        bytes.push_back(byte);
    }

    return bytes;
}

std::vector<uint8_t> bytes_to_bits(const std::vector<uint8_t>& bytes)
{
    std::vector<uint8_t> bits;
    bits.reserve(bytes.size() * 8);

    for (uint8_t byte : bytes) {
        for (int i = 7; i >= 0; --i) {
            bits.push_back(static_cast<uint8_t>((byte >> i) & 1u));
        }
    }

    return bits;
}

uint16_t crc16_ccitt(const std::vector<uint8_t>& bytes)
{
    uint16_t crc = 0xFFFFu;

    for (uint8_t byte : bytes) {
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                                  : static_cast<uint16_t>(crc << 1);
        }
    }

    return crc;
}

std::vector<uint8_t> pack_payload_bytes(const std::string& text, size_t payload_bytes_capacity)
{
    if (payload_bytes_capacity < 5) {
        return {};
    }

    const size_t text_len = std::min(text.size(), payload_bytes_capacity - 5);
    std::vector<uint8_t> frame;
    frame.reserve(payload_bytes_capacity);

    frame.push_back(kMagic0);
    frame.push_back(kMagic1);
    frame.push_back(static_cast<uint8_t>(text_len));
    for (size_t i = 0; i < text_len; ++i) {
        frame.push_back(static_cast<uint8_t>(text[i]));
    }

    const uint16_t crc = crc16_ccitt(frame);
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFFu));
    frame.push_back(static_cast<uint8_t>(crc & 0xFFu));
    frame.resize(payload_bytes_capacity, 0u);

    return frame;
}
