#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace sn::protocol {

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), computed bitwise so the
// table does not cost flash on the node.
[[nodiscard]] constexpr std::uint16_t crc16(std::span<const std::uint8_t> data) {
    std::uint16_t crc = 0xFFFF;
    for (const std::uint8_t byte : data) {
        crc ^= static_cast<std::uint16_t>(static_cast<unsigned>(byte) << 8U);
        for (int bit = 0; bit < 8; ++bit) {
            const unsigned shifted = static_cast<unsigned>(crc) << 1U;
            crc = static_cast<std::uint16_t>((crc & 0x8000U) != 0U ? shifted ^ 0x1021U : shifted);
        }
    }
    return crc;
}

}  // namespace sn::protocol
