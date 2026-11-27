#include "gtest/gtest.h"
#include "pc2/fan_control.hpp"

TEST(PC2FanControl, SkipsFirstReportOnce) {
    PC2FanControl f;
    EXPECT_EQ(f.consume_skip(), true);
    EXPECT_EQ(f.consume_skip(), false);
}

// Captured (first_pc2_cap): case at 43-44 C -> 19 32 4b 64 7d 96 af c8 e1 fa ff ff
TEST(PC2FanControl, HotClimbsBy0x19ToFull) {
    PC2FanControl f;
    int temps[] = {43, 43, 43, 44, 44, 44, 44, 44, 44, 44, 44, 44};
    int want[]  = {0x19, 0x32, 0x4b, 0x64, 0x7d, 0x96, 0xaf, 0xc8, 0xe1, 0xfa, 0xff, 0xff};
    for (int i = 0; i < 12; i++) EXPECT_EQ((int)f.on_temperature(temps[i]), want[i]);
}

// Captured (BM5asAM): 35 C -> 80, 35 -> 7b, 36 (rising) -> 87, 36 -> 82 ...
TEST(PC2FanControl, InBandStartsAt0x80AndDecays) {
    PC2FanControl f;
    EXPECT_EQ((int)f.on_temperature(35), 0x80);
    EXPECT_EQ((int)f.on_temperature(35), 0x7b);
    EXPECT_EQ((int)f.on_temperature(36), 0x87);
    EXPECT_EQ((int)f.on_temperature(36), 0x82);
}

TEST(PC2FanControl, FullOnOverheatOrBadReading) {
    PC2FanControl f;
    f.on_temperature(35);
    EXPECT_EQ((int)f.on_temperature(47), 0xff);
    PC2FanControl g;
    EXPECT_EQ((int)g.on_temperature(-1), 0xff);
}

TEST(PC2FanControl, NeverBelowZero) {
    PC2FanControl f;
    f.on_temperature(30);
    for (int i = 0; i < 40; i++) f.on_temperature(30);
    EXPECT_EQ(f.duty(), 0);
}
