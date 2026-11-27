// SourceCenter tests: replay live captures of a BeoMaster 5 (original software) acting as source
// center for a BeoSound 3000 audio master, and check we answer like it did.
#include "gtest/gtest.h"
#include "masterlink/source_center.hpp"
#include "masterlink/source_center_live_vectors.inc"

#include <algorithm>
#include <sstream>

namespace {
    typedef std::vector<uint8_t> Bytes;

    struct FakeHost : public AudioMasterHost {
        std::vector<Bytes> sent;
        bool distributing = false, standby_seen = false;
        std::vector<uint8_t> started, keys;
        int stops = 0;
        void send_telegram(MasterlinkTelegram &t) override {
            Bytes b = t.serialize();
            b.resize(b.size() - 2);             // MLGW captures carry no checksum / EOT
            sent.push_back(b);
        }
        void set_ml_distribution(bool on) override { distributing = on; }
        void set_ml_listening(bool) override {}
        bool start_source(uint8_t src) override { started.push_back(src); return true; }
        void stop_source() override { stops++; }
        void source_key(uint8_t, uint8_t key) override { keys.push_back(key); }
        void link_standby() override { standby_seen = true; }
    };

    MasterlinkTelegram incoming(const Bytes &ml) {
        PC2Message frame = {0x60, (uint8_t)(ml.size() + 1), 0x00};
        frame.insert(frame.end(), ml.begin(), ml.end());
        frame.push_back(0x61);
        return MasterlinkTelegram(frame);
    }

    SourceCenter::clock::time_point at(double t) {
        return SourceCenter::clock::time_point(std::chrono::milliseconds((long long)(t * 1000) + 100000));
    }

    std::string hex(const Bytes &b) {
        std::ostringstream os;
        for (auto x : b) { char s[4]; snprintf(s, sizeof s, "%02x ", x); os << s; }
        return os.str();
    }

    std::string diff(std::vector<Bytes> expected, std::vector<Bytes> got) {
        std::sort(expected.begin(), expected.end());
        std::sort(got.begin(), got.end());
        std::vector<Bytes> missing, extra;
        std::set_difference(expected.begin(), expected.end(), got.begin(), got.end(), std::back_inserter(missing));
        std::set_difference(got.begin(), got.end(), expected.begin(), expected.end(), std::back_inserter(extra));
        std::ostringstream os;
        for (auto &m : missing) os << "\n    missing: " << hex(m);
        for (auto &e : extra) os << "\n    extra:   " << hex(e);
        return os.str();
    }

    std::vector<Bytes> replay(SourceCenter &sc, FakeHost &host, const std::vector<LiveTelegram> &w) {
        std::vector<Bytes> expected;
        host.sent.clear();
        for (auto &row : w) {
            sc.tick(at(row.t));
            if (row.out) expected.push_back(row.bytes);
            else { auto t = incoming(row.bytes); sc.handle(t, at(row.t)); }
        }
        sc.tick(at(w.back().t + 10));
        return expected;
    }

    // Put the source center into "distributing <src>" before a replay
    void distribute(SourceCenter &sc, FakeHost &host, uint8_t src) {
        auto req = incoming({0xc2, 0xc1, 0x01, 0x0b, src, 0x00, 0x00, 0x6c, 0x0a, 0x01,
                             0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x01, 0xff, 0x02, 0x00});
        sc.handle(req, at(-50));
        host.sent.clear();
    }
}

// Kitchen joins N.MUSIC while the office plays it; then the audio master sends "all standby" (§6)
TEST(SourceCenter, ReplaysKitchenJoinAndAllStandby) {
    FakeHost host;
    SourceCenter sc(host);
    distribute(sc, host, 0x7a);
    auto expected = replay(sc, host, live_kitchen_and_standby);
    EXPECT_EQ(diff(expected, host.sent), std::string("")) << diff(expected, host.sent);
    EXPECT_EQ(host.standby_seen, true);
    EXPECT_EQ(sc.distributing(), false);
}

// Kitchen: N.MUSIC from standby -> N.RADIO -> the audio master's CD -> off (§6)
TEST(SourceCenter, ReplaysKitchenSourceChanges) {
    FakeHost host;
    SourceCenter sc(host);
    auto expected = replay(sc, host, live_kitchen_sources);
    EXPECT_EQ(diff(expected, host.sent), std::string("")) << diff(expected, host.sent);
    EXPECT_EQ(host.started, (std::vector<uint8_t>{0x7a, 0xa1}));
    EXPECT_EQ(sc.distributing(), false);
}

// Panel picks N.MUSIC, then N.RADIO: the video master asks the audio master, which orders us (§7)
TEST(SourceCenter, ReplaysPanelViaVideoMaster) {
    FakeHost host;
    SourceCenter sc(host);
    sc.set_local_playback(true);
    auto expected = replay(sc, host, live_panel);
    EXPECT_EQ(diff(expected, host.sent), std::string("")) << diff(expected, host.sent);
    EXPECT_EQ(sc.current_source(), 0xa1);
}

// TV takes the bus from our N.RADIO, then the kitchen takes it back for N.MUSIC (§8)
TEST(SourceCenter, ReplaysTvHandover) {
    FakeHost host;
    SourceCenter sc(host);
    sc.set_local_playback(true);
    distribute(sc, host, 0xa1);
    auto expected = replay(sc, host, live_tv);
    EXPECT_EQ(diff(expected, host.sent), std::string("")) << diff(expected, host.sent);
}

TEST(SourceCenter, RemoteKeyMatchesPanelTelegram) {
    // captured: c0 c1 01 0a 00 47 00 20 05 02 00 01 ff ff 94  ("N.Music" to the video master)
    FakeHost host;
    SourceCenter sc(host);
    sc.send_remote_key(0xc0, 0x94);
    ASSERT_TRUE(host.sent.size() == 1);
    EXPECT_EQ(host.sent[0], (Bytes{0xc0, 0xc1, 0x01, 0x0a, 0x00, 0x47, 0x00, 0x20, 0x05, 0x02,
                                   0x00, 0x01, 0xff, 0xff, 0x94}));
}

TEST(SourceCenter, IdleStatusOnlyAfterDelayAndWithoutLocalPlayback) {
    FakeHost host;
    SourceCenter sc(host);
    distribute(sc, host, 0x7a);
    auto stop = incoming({0xc2, 0xc1, 0x01, 0x0a, 0x7a, 0x00, 0x00, 0x10, 0x00, 0x03});
    sc.handle(stop, at(0));
    sc.tick(at(1));
    EXPECT_EQ(host.sent.size(), 0u);
    sc.tick(at(2.1));
    EXPECT_EQ(host.sent.size(), 1u);     // idle SOURCE_STATUS 47
}
