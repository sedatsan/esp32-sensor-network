#include "sn/protocol/report.hpp"

#include "sn/protocol/crc16.hpp"

namespace sn::protocol {
namespace {

template <typename T>
void putLe(ReportBuffer& buf, std::size_t offset, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        buf[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

template <typename T>
T getLe(std::span<const std::uint8_t> buf, std::size_t offset) {
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value = static_cast<T>(value | static_cast<T>(static_cast<T>(buf[offset + i]) << (8 * i)));
    }
    return value;
}

constexpr std::size_t kCrcOffset = kReportSize - 2;

}  // namespace

ReportBuffer encode(const SensorReport& report) {
    ReportBuffer buf{};
    buf[0] = kProtocolVersion;
    putLe(buf, 1, report.nodeId);
    putLe(buf, 5, report.sequence);
    putLe(buf, 9, report.distanceMm);
    putLe(buf, 11, report.batteryMv);
    buf[13] = static_cast<std::uint8_t>(report.flags);
    putLe(buf, kCrcOffset, crc16(std::span{buf}.first(kCrcOffset)));
    return buf;
}

Expected<SensorReport, DecodeError> decode(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != kReportSize) {
        return unexpected(DecodeError::WrongSize);
    }
    if (crc16(bytes.first(kCrcOffset)) != getLe<std::uint16_t>(bytes, kCrcOffset)) {
        return unexpected(DecodeError::BadCrc);
    }
    if (bytes[0] != kProtocolVersion) {
        return unexpected(DecodeError::UnsupportedVersion);
    }
    SensorReport report;
    report.nodeId = getLe<std::uint32_t>(bytes, 1);
    report.sequence = getLe<std::uint32_t>(bytes, 5);
    report.distanceMm = getLe<std::uint16_t>(bytes, 9);
    report.batteryMv = getLe<std::uint16_t>(bytes, 11);
    report.flags = static_cast<ReportFlags>(bytes[13]);
    return report;
}

}  // namespace sn::protocol
