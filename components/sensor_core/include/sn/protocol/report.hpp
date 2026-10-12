#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "sn/expected.hpp"

namespace sn::protocol {

inline constexpr std::uint8_t kProtocolVersion = 1;

enum class ReportFlags : std::uint8_t {
    None = 0,
    Motion = 1U << 0,
    DistanceValid = 1U << 1,
    ColdBoot = 1U << 2,
};

[[nodiscard]] constexpr ReportFlags operator|(ReportFlags a, ReportFlags b) {
    return static_cast<ReportFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
[[nodiscard]] constexpr bool hasFlag(ReportFlags value, ReportFlags flag) {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

// One reading sent from a sensor node to the gateway.
struct SensorReport {
    std::uint32_t nodeId = 0;    // low 32 bits of the node's MAC
    std::uint32_t sequence = 0;  // monotonically increasing, survives deep sleep
    std::uint16_t distanceMm = 0;
    std::uint16_t batteryMv = 0;
    ReportFlags flags = ReportFlags::None;

    friend bool operator==(const SensorReport&, const SensorReport&) = default;
};

// Wire format, little-endian, fixed size:
//   [0] version  [1..4] nodeId  [5..8] sequence  [9..10] distanceMm
//   [11..12] batteryMv  [13] flags  [14..15] CRC-16 over bytes 0..13
inline constexpr std::size_t kReportSize = 16;
using ReportBuffer = std::array<std::uint8_t, kReportSize>;

enum class DecodeError : std::uint8_t { WrongSize, UnsupportedVersion, BadCrc };

[[nodiscard]] ReportBuffer encode(const SensorReport& report);
[[nodiscard]] Expected<SensorReport, DecodeError> decode(std::span<const std::uint8_t> bytes);

}  // namespace sn::protocol
