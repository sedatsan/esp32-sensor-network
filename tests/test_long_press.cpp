#include <gtest/gtest.h>

#include "sn/app/long_press.hpp"

using sn::app::LongPressDetector;

TEST(LongPress, FiresOnceAfterHoldTime) {
    LongPressDetector d{3000};
    EXPECT_FALSE(d.update(true, 1000));
    EXPECT_FALSE(d.update(true, 3999));
    EXPECT_TRUE(d.update(true, 4000));
    EXPECT_FALSE(d.update(true, 9000));  // still held: no repeat
}

TEST(LongPress, ShortPressesDoNotAccumulate) {
    LongPressDetector d{3000};
    EXPECT_FALSE(d.update(true, 0));
    EXPECT_FALSE(d.update(true, 2000));
    EXPECT_FALSE(d.update(false, 2100));
    EXPECT_FALSE(d.update(true, 2200));
    EXPECT_FALSE(d.update(true, 5100));
    EXPECT_TRUE(d.update(true, 5200));
}

TEST(LongPress, FiresAgainAfterRelease) {
    LongPressDetector d{100};
    EXPECT_FALSE(d.update(true, 0));
    EXPECT_TRUE(d.update(true, 100));
    EXPECT_FALSE(d.update(false, 150));
    EXPECT_FALSE(d.update(true, 200));
    EXPECT_TRUE(d.update(true, 300));
}

TEST(LongPress, SurvivesMillisecondCounterWrap) {
    LongPressDetector d{100};
    EXPECT_FALSE(d.update(true, 0xFFFFFFF0U));
    EXPECT_TRUE(d.update(true, 0x00000060U));
}
