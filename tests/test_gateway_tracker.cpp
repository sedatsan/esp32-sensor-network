#include <gtest/gtest.h>

#include "sn/app/gateway_tracker.hpp"

using sn::app::GatewayTracker;
using sn::protocol::ReportFlags;
using sn::protocol::SensorReport;
using Outcome = GatewayTracker::Outcome;

namespace {
SensorReport report(std::uint32_t node, std::uint32_t seq, ReportFlags flags = ReportFlags::None) {
    return SensorReport{.nodeId = node, .sequence = seq, .flags = flags};
}
}  // namespace

TEST(GatewayTracker, ClassifiesSequenceProgression) {
    GatewayTracker t;
    EXPECT_EQ(t.onReport(report(1, 10)).outcome, Outcome::NewNode);
    EXPECT_EQ(t.onReport(report(1, 11)).outcome, Outcome::InOrder);
    EXPECT_EQ(t.onReport(report(1, 11)).outcome, Outcome::Duplicate);
    EXPECT_EQ(t.onReport(report(1, 9)).outcome, Outcome::Duplicate);  // late retransmit

    const auto gap = t.onReport(report(1, 15));
    EXPECT_EQ(gap.outcome, Outcome::Gap);
    EXPECT_EQ(gap.lostNow, 3U);

    const auto* stats = t.find(1);
    ASSERT_NE(stats, nullptr);
    EXPECT_EQ(stats->received, 3U);
    EXPECT_EQ(stats->lost, 3U);
    EXPECT_EQ(stats->duplicates, 2U);
}

TEST(GatewayTracker, HandlesWrapAround) {
    GatewayTracker t;
    (void)t.onReport(report(1, UINT32_MAX));
    EXPECT_EQ(t.onReport(report(1, 0)).outcome, Outcome::InOrder);
}

TEST(GatewayTracker, ColdBootResetsSequence) {
    GatewayTracker t;
    (void)t.onReport(report(1, 500));
    EXPECT_EQ(t.onReport(report(1, 0, ReportFlags::ColdBoot)).outcome, Outcome::Restarted);
    EXPECT_EQ(t.onReport(report(1, 1)).outcome, Outcome::InOrder);
    EXPECT_EQ(t.find(1)->restarts, 1U);
}

TEST(GatewayTracker, TracksNodesIndependentlyUpToCapacity) {
    GatewayTracker t;
    for (std::uint32_t id = 0; id < GatewayTracker::kMaxNodes; ++id) {
        EXPECT_EQ(t.onReport(report(id, 0)).outcome, Outcome::NewNode);
    }
    EXPECT_EQ(t.onReport(report(999, 0)).outcome, Outcome::TableFull);
    EXPECT_EQ(t.onReport(report(3, 1)).outcome, Outcome::InOrder);
    EXPECT_EQ(t.nodeCount(), GatewayTracker::kMaxNodes);
}
