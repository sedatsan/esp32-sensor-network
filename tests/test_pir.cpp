#include <gtest/gtest.h>

#include "fakes.hpp"
#include "sn/drivers/pir.hpp"

using sn::drivers::Pir;
using sn::test::FakeClock;
using sn::test::FakeInput;

namespace {

struct PirTest : ::testing::Test {
    FakeClock clock{0};
    FakeInput out;
    Pir pir{out, clock, Pir::Config{.warmupUs = 1'000}};
};

}  // namespace

TEST_F(PirTest, IgnoresOutputDuringWarmup) {
    out.level = true;
    EXPECT_FALSE(pir.isReady());
    EXPECT_FALSE(pir.motionDetected());
    EXPECT_EQ(pir.poll(), std::nullopt);
}

TEST_F(PirTest, ReportsEdgesAfterWarmup) {
    clock.advance(1'000);
    EXPECT_EQ(pir.poll(), std::nullopt);
    out.level = true;
    EXPECT_EQ(pir.poll(), Pir::Event::MotionStarted);
    EXPECT_EQ(pir.poll(), std::nullopt);  // no repeat while held high
    out.level = false;
    EXPECT_EQ(pir.poll(), Pir::Event::MotionEnded);
}

TEST_F(PirTest, SkipWarmupAfterDeepSleep) {
    pir.skipWarmup();
    out.level = true;
    EXPECT_TRUE(pir.motionDetected());
    EXPECT_EQ(pir.poll(), Pir::Event::MotionStarted);
}
