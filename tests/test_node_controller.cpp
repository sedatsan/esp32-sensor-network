#include <gtest/gtest.h>

#include <vector>

#include "sn/app/node_controller.hpp"

using namespace sn::app;
using sn::protocol::decode;
using sn::protocol::hasFlag;
using sn::protocol::ReportFlags;

namespace {

struct StubSensors : ISensors {
    bool motion() override { return motionValue; }
    std::uint16_t distanceMm() override { return distanceValue; }
    std::uint16_t batteryMv() override { return 3700; }
    bool motionValue = false;
    std::uint16_t distanceValue = 1500;
};

struct ScriptedTransport : ITransport {
    bool send(std::span<const std::uint8_t> payload) override {
        sent.emplace_back(payload.begin(), payload.end());
        const bool ok = failuresLeft == 0;
        if (!ok) {
            --failuresLeft;
        }
        return ok;
    }
    std::vector<std::vector<std::uint8_t>> sent;
    int failuresLeft = 0;
};

struct RecordingDelay : IClockDelay {
    void delayMs(std::uint32_t ms) override { delays.push_back(ms); }
    std::vector<std::uint32_t> delays;
};

struct NodeControllerTest : ::testing::Test {
    StubSensors sensors;
    ScriptedTransport transport;
    RecordingDelay delay;
    NodeConfig config{.nodeId = 99, .reportIntervalS = 60};
    NodeController controller{sensors, transport, delay, config};
    RetainedState state;
};

}  // namespace

TEST_F(NodeControllerTest, SendsDecodableReportAndIncrementsSequence) {
    state.sequence = 41;
    const auto plan = controller.runCycle(WakeCause::Timer, state);

    ASSERT_EQ(transport.sent.size(), 1U);
    const auto report = decode(transport.sent[0]);
    ASSERT_TRUE(report);
    EXPECT_EQ(report->nodeId, 99U);
    EXPECT_EQ(report->sequence, 41U);
    EXPECT_EQ(report->distanceMm, 1500);
    EXPECT_TRUE(hasFlag(report->flags, ReportFlags::DistanceValid));
    EXPECT_FALSE(hasFlag(report->flags, ReportFlags::ColdBoot));
    EXPECT_EQ(state.sequence, 42U);
    EXPECT_EQ(plan.timerWakeS, 60U);
    EXPECT_TRUE(plan.wakeOnMotion);
}

TEST_F(NodeControllerTest, FlagsColdBootAndInvalidDistance) {
    sensors.distanceValue = 0;
    (void)controller.runCycle(WakeCause::ColdBoot, state);
    const auto report = decode(transport.sent.at(0));
    ASSERT_TRUE(report);
    EXPECT_TRUE(hasFlag(report->flags, ReportFlags::ColdBoot));
    EXPECT_FALSE(hasFlag(report->flags, ReportFlags::DistanceValid));
}

TEST_F(NodeControllerTest, DisablesMotionWakeWhilePirIsHigh) {
    sensors.motionValue = true;
    const auto plan = controller.runCycle(WakeCause::Motion, state);
    EXPECT_FALSE(plan.wakeOnMotion);
    const auto report = decode(transport.sent.at(0));
    ASSERT_TRUE(report);
    EXPECT_TRUE(hasFlag(report->flags, ReportFlags::Motion));
}

TEST_F(NodeControllerTest, RetriesWithExponentialBackoff) {
    transport.failuresLeft = 2;
    (void)controller.runCycle(WakeCause::Timer, state);
    EXPECT_EQ(transport.sent.size(), 3U);
    EXPECT_EQ(delay.delays, (std::vector<std::uint32_t>{20, 40}));
    EXPECT_EQ(state.failedReports, 0U);
}

TEST_F(NodeControllerTest, CountsReportThatNeverGotThrough) {
    transport.failuresLeft = 10;
    (void)controller.runCycle(WakeCause::Timer, state);
    EXPECT_EQ(transport.sent.size(), 3U);
    EXPECT_EQ(state.failedReports, 1U);
    EXPECT_EQ(state.sequence, 1U);  // sequence still advances so gaps are visible
}

TEST_F(NodeControllerTest, ColdBootSleepsShortlyWithoutMotionWake) {
    NodeController slow{sensors, transport, delay,
                        NodeConfig{.reportIntervalS = 600, .pirWarmupS = 60}};
    const auto plan = slow.runCycle(WakeCause::ColdBoot, state);
    EXPECT_EQ(plan.timerWakeS, 60U);
    EXPECT_FALSE(plan.wakeOnMotion);
}
