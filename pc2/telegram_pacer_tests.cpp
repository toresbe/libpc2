#include "gtest/gtest.h"
#include "pc2/telegram_pacer.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {
    struct Recorder {
        std::mutex m;
        std::vector<PC2Message> sent;
        void operator()(const PC2Message &msg) { std::scoped_lock lk(m); sent.push_back(msg); }
        size_t count() { std::scoped_lock lk(m); return sent.size(); }
    };

    template <typename F> bool eventually(F pred, std::chrono::milliseconds limit = 1000ms) {
        auto end = std::chrono::steady_clock::now() + limit;
        while (std::chrono::steady_clock::now() < end) {
            if (pred()) return true;
            std::this_thread::sleep_for(2ms);
        }
        return pred();
    }
}

TEST(PC2TelegramPacer, SendsOneTelegramAtATime) {
    Recorder rec;
    PC2TelegramPacer pacer([&rec](const PC2Message &m) { rec(m); }, 5000ms);
    pacer.enqueue({0xe0, 0x01});
    pacer.enqueue({0xe0, 0x02});
    pacer.enqueue({0xe0, 0x03});

    ASSERT_TRUE(eventually([&] { return rec.count() == 1; }));
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(rec.count(), 1u) << "second telegram must wait for the PC2's status answer";

    pacer.acknowledge();
    ASSERT_TRUE(eventually([&] { return rec.count() == 2; }));
    pacer.acknowledge();
    ASSERT_TRUE(eventually([&] { return rec.count() == 3; }));

    EXPECT_EQ(rec.sent[0], (PC2Message{0xe0, 0x01}));
    EXPECT_EQ(rec.sent[1], (PC2Message{0xe0, 0x02}));
    EXPECT_EQ(rec.sent[2], (PC2Message{0xe0, 0x03}));
    EXPECT_EQ(pacer.timeouts(), 0u);
}

TEST(PC2TelegramPacer, TimeoutReleasesNextTelegram) {
    Recorder rec;
    PC2TelegramPacer pacer([&rec](const PC2Message &m) { rec(m); }, 30ms);
    pacer.enqueue({0xe0, 0x01});
    pacer.enqueue({0xe0, 0x02});
    ASSERT_TRUE(eventually([&] { return rec.count() == 2; }));
    EXPECT_GE(pacer.timeouts(), 1u);
}

TEST(PC2TelegramPacer, StrayAcknowledgeDoesNotSkipAhead) {
    Recorder rec;
    PC2TelegramPacer pacer([&rec](const PC2Message &m) { rec(m); }, 5000ms);
    pacer.acknowledge();               // nothing outstanding: ignored
    pacer.enqueue({0xe0, 0x01});
    pacer.enqueue({0xe0, 0x02});
    ASSERT_TRUE(eventually([&] { return rec.count() == 1; }));
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(rec.count(), 1u);
}
