#include <GUnit.h>
#include "GUnit/GMock.h"
#include "gmock/gmock.h"

#include <thread>
#include <chrono>

#include "pc2/mailbox.hpp"

using ::testing::StrictGMock;

GTEST(PC2Mailbox) {
    SHOULD("Construct and destruct") {
        PC2Mailbox mbox;
    }

    SHOULD("Have zero messages on construct") {
        PC2Mailbox mbox;

        EXPECT_EQ(0, mbox.count());
    }

    SHOULD("Have count == 1 when a message is added") {
        PC2Mailbox mbox;
        PC2Message tgram;
        mbox.push(tgram);
        EXPECT_EQ(1, mbox.count());
    }

    SHOULD("Get the same message back out and have count return to 0") {
        PC2Mailbox mbox;
        PC2Message tgram = { 0x00 };
        mbox.push(tgram);
        auto tgram2 = mbox.pop_sync();

        EXPECT_EQ(tgram, tgram2);
        EXPECT_EQ(0, mbox.count());
    }

    SHOULD("Block if empty until a message arrives") {
        using namespace std::chrono_literals;

        PC2Mailbox mbox;

        bool has_sent_message;

        std::thread tgram_sender([&mbox, &has_sent_message]() {
                PC2Message tgram = { 0x00 };
                std::this_thread::sleep_for(120ms);
                mbox.push(tgram);
                has_sent_message = true;
            }
        );

        std::thread tgram_receiver([&mbox]() {
                auto foo = mbox.pop_sync(); 
            }
        );

        tgram_receiver.join();
        ASSERT_EQ(true, has_sent_message);
        tgram_sender.join();
    }
}
