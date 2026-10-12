#include <gtest/gtest.h>

#include "sn/protocol/crc16.hpp"
#include "sn/protocol/report.hpp"

using namespace sn::protocol;

TEST(Crc16, MatchesCcittFalseCheckValue) {
    constexpr std::array<std::uint8_t, 9> kCheck{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    static_assert(crc16(kCheck) == 0x29B1);
    EXPECT_EQ(crc16(kCheck), 0x29B1);
}

TEST(Report, RoundTrips) {
    const SensorReport in{.nodeId = 0xA1B2C3D4,
                          .sequence = 123456,
                          .distanceMm = 1234,
                          .batteryMv = 3712,
                          .flags = ReportFlags::Motion | ReportFlags::DistanceValid};
    const auto bytes = encode(in);
    const auto out = decode(bytes);
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, in);
}

TEST(Report, IsLittleEndianWithVersionFirst) {
    const auto bytes = encode(SensorReport{.nodeId = 0x04030201});
    EXPECT_EQ(bytes[0], kProtocolVersion);
    EXPECT_EQ(bytes[1], 0x01);
    EXPECT_EQ(bytes[4], 0x04);
}

TEST(Report, RejectsWrongSize) {
    const auto bytes = encode(SensorReport{});
    const auto out = decode(std::span{bytes}.first(kReportSize - 1));
    ASSERT_FALSE(out);
    EXPECT_EQ(out.error(), DecodeError::WrongSize);
}

TEST(Report, DetectsEveryFlippedBit) {
    const auto good = encode(SensorReport{.nodeId = 7, .sequence = 42, .distanceMm = 900});
    for (std::size_t bit = 0; bit < kReportSize * 8; ++bit) {
        auto bad = good;
        bad[bit / 8] ^= static_cast<std::uint8_t>(1U << (bit % 8));
        const auto out = decode(bad);
        ASSERT_FALSE(out) << "bit " << bit;
        EXPECT_EQ(out.error(), DecodeError::BadCrc) << "bit " << bit;
    }
}

TEST(Report, RejectsFutureVersionWithValidCrc) {
    auto bytes = encode(SensorReport{});
    bytes[0] = kProtocolVersion + 1;
    const auto crc = crc16(std::span{bytes}.first(kReportSize - 2));
    bytes[kReportSize - 2] = static_cast<std::uint8_t>(crc);
    bytes[kReportSize - 1] = static_cast<std::uint8_t>(crc >> 8);
    const auto out = decode(bytes);
    ASSERT_FALSE(out);
    EXPECT_EQ(out.error(), DecodeError::UnsupportedVersion);
}
