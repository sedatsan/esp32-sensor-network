#include <gtest/gtest.h>

#include "fakes.hpp"
#include "sn/drivers/hcsr04.hpp"

using sn::drivers::echoUsToMm;
using sn::drivers::Hcsr04;
using sn::test::FakeClock;
using sn::test::FakeHcsr04;

namespace {

// Echo time that corresponds to `mm` at 20 C.
std::uint64_t echoFor(std::uint32_t mm) {
    return static_cast<std::uint64_t>(mm * 2.0 / 0.34342 + 0.5);
}

struct Hcsr04Test : ::testing::Test {
    FakeClock clock;
    FakeHcsr04 sensor{clock};
    Hcsr04 driver{sensor, sensor, clock};
};

}  // namespace

TEST(EchoConversion, MatchesSpeedOfSoundAt20C) {
    // 343.42 m/s -> 1000 us round trip = 171.7 mm
    EXPECT_EQ(echoUsToMm(1000, 20.0F), 172U);
    EXPECT_EQ(echoUsToMm(0, 20.0F), 0U);
}

TEST(EchoConversion, CompensatesForTemperature) {
    EXPECT_LT(echoUsToMm(10'000, 0.0F), echoUsToMm(10'000, 30.0F));
}

TEST_F(Hcsr04Test, MeasuresDistance) {
    sensor.echoes.push_back(echoFor(500));
    const auto result = driver.measureMm();
    ASSERT_TRUE(result);
    EXPECT_NEAR(*result, 500, 2);
    EXPECT_EQ(sensor.triggers, 1);
}

TEST_F(Hcsr04Test, ReportsMissingSensor) {
    sensor.echoes.push_back(std::nullopt);
    const auto result = driver.measureMm();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Hcsr04::Error::EchoStartTimeout);
}

TEST_F(Hcsr04Test, ReportsEchoThatNeverEnds) {
    sensor.echoes.push_back(FakeHcsr04::kStuckHigh);
    const auto result = driver.measureMm();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Hcsr04::Error::EchoEndTimeout);
}

TEST_F(Hcsr04Test, RejectsDistanceBelowMinimum) {
    sensor.echoes.push_back(echoFor(5));
    const auto result = driver.measureMm();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Hcsr04::Error::OutOfRange);
}

TEST_F(Hcsr04Test, MedianRejectsOutliers) {
    for (const std::uint32_t mm : {800U, 802U, 3900U, 798U, 30U}) {
        sensor.echoes.push_back(echoFor(mm));
    }
    const auto result = driver.measureMedianMm(5);
    ASSERT_TRUE(result);
    EXPECT_NEAR(*result, 800, 3);
    EXPECT_EQ(sensor.triggers, 5);
}

TEST_F(Hcsr04Test, MedianIgnoresFailedSamplesWhenMajorityIsValid) {
    sensor.echoes = {echoFor(1000), std::nullopt, echoFor(1004), echoFor(996), std::nullopt};
    const auto result = driver.measureMedianMm(5);
    ASSERT_TRUE(result);
    EXPECT_NEAR(*result, 1000, 3);
}

TEST_F(Hcsr04Test, MedianFailsWithoutMajority) {
    sensor.echoes = {echoFor(1000), std::nullopt, std::nullopt, echoFor(1000)};
    const auto result = driver.measureMedianMm(4);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Hcsr04::Error::EchoStartTimeout);
}

TEST_F(Hcsr04Test, WaitsBetweenMeasurements) {
    sensor.echoes = {echoFor(500), echoFor(500), echoFor(500)};
    const std::uint64_t start = clock.peek();
    (void)driver.measureMedianMm(3);
    EXPECT_GE(clock.peek() - start, 2 * Hcsr04::kInterMeasurementDelayUs);
}
