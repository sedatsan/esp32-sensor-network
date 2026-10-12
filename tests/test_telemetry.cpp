#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "sn/app/telemetry.hpp"

using namespace sn::app;
using sn::protocol::ReportFlags;
using sn::protocol::SensorReport;

namespace {

const SensorReport kReport{.nodeId = 0xD51EB380,
                           .sequence = 1,
                           .distanceMm = 172,
                           .batteryMv = 0,
                           .flags = ReportFlags::Motion | ReportFlags::DistanceValid};

}  // namespace

TEST(Telemetry, FormatsReportAsJson) {
    std::array<char, 256> buf{};
    const auto len = formatReportJson(
        kReport, {.rssi = -69, .outcome = GatewayTracker::Outcome::InOrder, .lostTotal = 0}, buf);
    EXPECT_EQ(std::string_view(buf.data(), len),
              R"({"node":"d51eb380","seq":1,"distance_mm":172,"distance_valid":true,)"
              R"("motion":true,"cold_boot":false,"battery_mv":0,"rssi":-69,"link":"ok","lost":0})");
    EXPECT_EQ(buf[len], '\0');
}

TEST(Telemetry, FormatsExtremeValues) {
    const SensorReport report{.nodeId = 0x0000000F,
                              .sequence = 0xFFFFFFFF,
                              .distanceMm = 0xFFFF,
                              .batteryMv = 0xFFFF,
                              .flags = ReportFlags::ColdBoot};
    std::array<char, 256> buf{};
    const auto len = formatReportJson(
        report, {.rssi = -128, .outcome = GatewayTracker::Outcome::Gap, .lostTotal = 7}, buf);
    const std::string_view json(buf.data(), len);
    EXPECT_NE(json.find(R"("node":"0000000f")"), std::string_view::npos);
    EXPECT_NE(json.find(R"("seq":4294967295)"), std::string_view::npos);
    EXPECT_NE(json.find(R"("rssi":-128)"), std::string_view::npos);
    EXPECT_NE(json.find(R"("cold_boot":true)"), std::string_view::npos);
    EXPECT_NE(json.find(R"("link":"gap","lost":7})"), std::string_view::npos);
}

TEST(Telemetry, ReturnsZeroWhenBufferTooSmall) {
    std::array<char, 256> full{};
    const auto needed = formatReportJson(kReport, {}, full);
    ASSERT_GT(needed, 0U);

    std::array<char, 256> buf{};
    buf.fill('x');
    EXPECT_EQ(formatReportJson(kReport, {}, std::span{buf}.first(needed)), 0U);  // no room for NUL
    EXPECT_EQ(buf[0], '\0');
    EXPECT_EQ(formatReportJson(kReport, {}, std::span{buf}.first(needed + 1)), needed);
    EXPECT_EQ(formatReportJson(kReport, {}, std::span<char>{}), 0U);
}

TEST(Telemetry, BuildsTopics) {
    std::array<char, 64> buf{};
    auto len = formatNodeTopic("sensornet", 0x0E3E84, 0xD51EB380, buf);
    EXPECT_EQ(std::string_view(buf.data(), len), "sensornet/000e3e84/node/d51eb380");
    len = formatStatusTopic("sensornet", 0xF90E3E84, buf);
    EXPECT_EQ(std::string_view(buf.data(), len), "sensornet/f90e3e84/status");
}

TEST(Telemetry, NamesEveryOutcome) {
    using O = GatewayTracker::Outcome;
    for (const auto o :
         {O::NewNode, O::InOrder, O::Gap, O::Duplicate, O::Restarted, O::TableFull}) {
        EXPECT_NE(outcomeName(o), "unknown");
    }
}
