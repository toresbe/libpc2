#include "gtest/gtest.h"
#include "pc2/wakeup.hpp"
#include "pc2/fragment_assembler.hpp"

TEST(PC2Wakeup, RecognisesFirmwarePackets) {
    EXPECT_EQ(pc2_wakeup_packet({0xff, 0xff, 0xff, 0xff}) == PC2Wakeup::by_remote, true);
    EXPECT_EQ(pc2_wakeup_packet({0xff, 0xfc, 0xfe, 0xff}) == PC2Wakeup::bus_resumed, true);
}

TEST(PC2Wakeup, IgnoresEverythingElse) {
    EXPECT_EQ(pc2_wakeup_packet({0xff, 0xff, 0x01, 0xff, 0xff}) == PC2Wakeup::none, true);  // the "bogus" message
    EXPECT_EQ(pc2_wakeup_packet({0x60, 0x02, 0x0a, 0x00, 0x61}) == PC2Wakeup::none, true);
    EXPECT_EQ(pc2_wakeup_packet({0xff, 0xff, 0xff}) == PC2Wakeup::none, true);
}

TEST(PC2Wakeup, AssemblerReportsMessageInProgress) {
    PC2MessageFragmentAssembler a;
    EXPECT_EQ(a.in_progress(), false);
    a << PC2Message{0x60, 0x0d, 0x00, 0xc1, 0xc0, 0x01, 0x14};     // first 7 of 16 bytes
    EXPECT_EQ(a.in_progress(), true);
    a << PC2Message{0x00, 0x00, 0x00, 0x04, 0x02, 0x04, 0x02};
    a << PC2Message{0x01, 0x61};
    EXPECT_EQ(a.in_progress(), false);
    EXPECT_EQ(a.has_complete_message(), true);
}
