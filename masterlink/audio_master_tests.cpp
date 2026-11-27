// AudioMaster tests: replay conversations captured from B&O's own audio master (a BeoSound 5)
// and check that we send the same Masterlink telegrams, byte for byte.
#include "gtest/gtest.h"
#include "masterlink/audio_master.hpp"
#include "masterlink/audio_master_capture_vectors.inc"

#include <algorithm>
#include <functional>
#include <map>
#include <sstream>

namespace {
    typedef std::vector<uint8_t> Bytes;

    struct FakeHost : public AudioMasterHost {
        std::vector<Bytes> sent;
        bool distributing = false, listening = false, standby_seen = false;
        std::vector<uint8_t> started_sources;
        int stops = 0;
        void send_telegram(MasterlinkTelegram &t) override { sent.push_back(t.serialize()); }
        void set_ml_distribution(bool on) override { distributing = on; }
        void set_ml_listening(bool on) override { listening = on; }
        bool start_source(uint8_t src) override { started_sources.push_back(src); return true; }
        void stop_source() override { stops++; }
        void link_standby() override { standby_seen = true; }
        int lock_requests = 0, lock_releases = 0;
        void request_bus_lock() override { lock_requests++; }
        void release_bus_lock() override { lock_releases++; }
        std::tm local_time() override { std::tm t{}; t.tm_hour = 14; t.tm_min = 42; t.tm_sec = 28;
                                        t.tm_mday = 25; t.tm_mon = 11; t.tm_year = 118; return t; }
    };

    MasterlinkTelegram incoming(const Bytes &ml) {
        PC2Message frame = {0x60, (uint8_t)(ml.size() + 1), 0x00};
        frame.insert(frame.end(), ml.begin(), ml.end());
        frame.push_back(0x61);
        return MasterlinkTelegram(frame);
    }

    AudioMaster::clock::time_point at(double t) {
        return AudioMaster::clock::time_point(std::chrono::milliseconds((long long)(t * 1000)));
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

    /*! Feed the IN telegrams of a captured window; run `actions` (local API calls) at their
     *  times; return what we sent and what B&O's software sent in the same window. */
    std::pair<std::vector<Bytes>, std::vector<Bytes>>
    replay(AudioMaster &am, FakeHost &host, const std::vector<CapturedTelegram> &window,
           std::map<double, std::function<void()>> actions = {}) {
        std::vector<Bytes> expected;
        host.sent.clear();
        for (auto &row : window) {
            while (!actions.empty() && actions.begin()->first <= row.t) {
                actions.begin()->second();
                actions.erase(actions.begin());
            }
            am.tick(at(row.t));
            if (row.out) expected.push_back(row.bytes);
            else { auto t = incoming(row.bytes); am.handle(t, at(row.t)); }
        }
        for (auto &a : actions) a.second();
        return {host.sent, expected};
    }
}

TEST(AudioMaster, SerializedChecksumMatchesCapture) {
    // REQUEST LINK_INFO to the video master, as captured: ... 9a 00
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    am.listen_to_video_source(0x0b);
    ASSERT_TRUE(host.sent.size() >= 1);
    EXPECT_EQ(host.sent[0], (Bytes{0xc0, 0xc1, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x01, 0x01, 0x00, 0x9a, 0x00}));
}

TEST(AudioMaster, LinkInfoRequestFollowsSystemConfiguration) {
    // SystemConfiguration 5 (what libpc2 sends in audio-master mode): "01 02 00", as in every
    // configuration 5 USB capture (e.g. PC2_AM_ML_source_changes, 0.656 s)
    FakeHost host;
    AudioMaster am(host);
    am.start(AudioMaster::clock::time_point{});
    ASSERT_EQ(host.sent.size(), 3u);
    EXPECT_EQ(host.sent[2], (Bytes{0xc0, 0xc1, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x01, 0x02, 0x00, 0x9b, 0x00}));
}

TEST(AudioMaster, ParsesDestSourceFromRightByte) {
    // VM -> PC_1 SOURCE_CONTROL with dest_src = 0x47 (captured)
    auto t = incoming({0xc2, 0xc0, 0x01, 0x0a, 0x47, 0x00, 0x00, 0x0d, 0x06, 0x01, 0x47, 0x1f, 0x02, 0x01, 0x02, 0x00});
    EXPECT_EQ(t.dest_src, 0x47);
    EXPECT_EQ(t.src_src, 0x00);
}

// Link rooms connect, switch source, leave and come back (capture 137 s - 420 s):
// bus arbitration with the video master, listener tracking, source change notices.
TEST(AudioMaster, ReplaysLinkRoomConversation) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    auto [got, expected] = replay(am, host, conversation_137_420);
    EXPECT_EQ(got.size(), expected.size()) << diff(expected, got);
    EXPECT_EQ(diff(expected, got), std::string("")) << diff(expected, got);
    EXPECT_EQ(am.state() == AudioMaster::BusState::distributing, true);
    EXPECT_EQ(am.current_source(), 0x8d);
    EXPECT_EQ(am.listeners(), (std::set<uint8_t>{0x01, 0x04}));
    EXPECT_EQ(host.distributing, true);
}

// The audio master picks TV (a video master source), then goes to standby (735 s - 780 s)
TEST(AudioMaster, ReplaysTvAndStandby) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    bool listening_before_standby = false;
    auto [got, expected] = replay(am, host, tv_735_780, {
        {735.9, [&] { am.listen_to_video_source(0x0b); }},
        {779.7, [&] { listening_before_standby = host.listening; am.local_standby(); }},
    });
    EXPECT_EQ(diff(expected, got), std::string("")) << diff(expected, got);
    EXPECT_EQ(listening_before_standby, true);
    EXPECT_EQ(host.listening, false);
    EXPECT_EQ(am.state() == AudioMaster::BusState::idle, true);
}

TEST(AudioMaster, ReplaysTvAndStandbySecondTime) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    auto [got, expected] = replay(am, host, tv_1627_1642, {
        {1627.4, [&] { am.listen_to_video_source(0x0b); }},
        {1641.1, [&] { am.local_standby(); }},
    });
    EXPECT_EQ(diff(expected, got), std::string("")) << diff(expected, got);
}

// A link room sends "all standby" while we distribute radio (1593 s)
TEST(AudioMaster, ReplaysAllStandbyFromLinkRoom) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    // pre-state: distributing RADIO (0x6f) to node 01, no video master answering
    auto req = incoming({0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x45, 0x07, 0x01, 0x02, 0x6f, 0x00, 0x02, 0x01, 0x00, 0x00});
    am.handle(req, at(1500));
    am.tick(at(1502));
    ASSERT_TRUE(am.state() == AudioMaster::BusState::distributing);
    auto [got, expected] = replay(am, host, standby_all_1593_1595);
    EXPECT_EQ(diff(expected, got), std::string("")) << diff(expected, got);
    EXPECT_EQ(host.standby_seen, true);
    EXPECT_EQ(host.distributing, false);
    EXPECT_EQ(am.listeners().empty(), true);
}

// While listening to the video master's TV, a room asks for A.MEM: we take the bus back (984 s)
TEST(AudioMaster, ReplaysTakingBusBackFromVideoMaster) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    am.listen_to_video_source(0x0b);
    auto confirm = incoming({0xc1, 0xc0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x44, 0x07, 0x05, 0x02, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00});
    am.handle(confirm, at(900));
    ASSERT_TRUE(am.state() == AudioMaster::BusState::listening_to_vm);
    auto [got, expected] = replay(am, host, amem_from_tv_984_986);
    EXPECT_EQ(diff(expected, got), std::string("")) << diff(expected, got);
    EXPECT_EQ(host.listening, false);
    EXPECT_EQ(am.current_source(), 0x79);
}

TEST(AudioMaster, StartupAndPeriodicClock) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    am.start(at(0));
    // idle SOURCE_STATUS from PC_1, CLOCK, LINK_INFO probe
    ASSERT_TRUE(host.sent.size() == 3);
    // clock as captured at 0.656 s: 80 c1 01 14 ... 40 0b 0b 0a 00 03 14 42 28 00 25 12 18 02 88 00
    EXPECT_EQ(host.sent[1], (Bytes{0x80, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x40, 0x0b, 0x0b, 0x0a, 0x00, 0x03,
                                   0x14, 0x42, 0x28, 0x00, 0x25, 0x12, 0x18, 0x02, 0x88, 0x00}));
    host.sent.clear();
    am.tick(at(400));
    EXPECT_EQ(host.sent.size(), 0u);
    am.tick(at(431));
    EXPECT_EQ(host.sent.size(), 1u);
}

TEST(AudioMaster, NoVideoMasterMeansTakeBusAfterTimeout) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    auto req = incoming({0xc1, 0x04, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x45, 0x07, 0x01, 0x02, 0x8d, 0x00, 0x02, 0x01, 0x00, 0x00});
    am.handle(req, at(10));
    EXPECT_EQ(am.state() == AudioMaster::BusState::arbitrating, true);
    am.tick(at(10.5));
    EXPECT_EQ(am.state() == AudioMaster::BusState::arbitrating, true);
    am.tick(at(11.1));
    EXPECT_EQ(am.state() == AudioMaster::BusState::distributing, true);
    EXPECT_EQ(am.listeners(), (std::set<uint8_t>{0x04}));
}

TEST(AudioMaster, LocalStandbyKeepsDistributingToListeners) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);   // 2018 captures: no lock traffic
    auto req = incoming({0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x45, 0x07, 0x01, 0x02, 0x8d, 0x00, 0x02, 0x01, 0x00, 0x00});
    am.handle(req, at(10));
    am.tick(at(12));
    am.local_standby();
    EXPECT_EQ(am.state() == AudioMaster::BusState::distributing, true);
    EXPECT_EQ(host.distributing, true);
}


// ---- Bus handover and lock, checked against what the BeoSound 3000 audio master sent (live, 2026-11-27)

namespace {
    // MLGW bytes have no checksum/EOT: compare without them
    Bytes strip(const Bytes &b) { return Bytes(b.begin(), b.end() - 2); }
    bool sent_contains(const FakeHost &h, const Bytes &mlgw) {
        for (auto &b : h.sent) if (strip(b) == mlgw) return true;
        return false;
    }
}

// The video master wants the bus for TV while we distribute N.RADIO (capture §8, 04:02:02)
TEST(AudioMasterHandover, VideoMasterTakesTheBus) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);
    auto req = incoming({0xc1, 0x04, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x45, 0x07, 0x01, 0x02, 0xa1, 0x00, 0x02, 0x01, 0x00, 0x00});
    am.handle(req, at(0));
    am.tick(at(2));                                   // no VM answer: take the bus
    ASSERT_TRUE(am.state() == AudioMaster::BusState::distributing);
    host.sent.clear();
    // VM: REQUEST 0x44 v3 "02 a1 00 02 01 00 03 01 00 00 00 02 0b"
    auto change = incoming({0xc1, 0xc0, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x44, 0x0d, 0x03,
                            0x02, 0xa1, 0x00, 0x02, 0x01, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x02, 0x0b});
    am.handle(change, at(3));
    // BS3000 answered: c0 c1 01 14 00 00 00 44 0d 07 02 a1 00 02 01 00 03 01 00 00 00 03 0b
    EXPECT_EQ(sent_contains(host, {0xc0, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x44, 0x0d, 0x07,
                                   0x02, 0xa1, 0x00, 0x02, 0x01, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x03, 0x0b}), true);
    host.sent.clear();
    auto claim = incoming({0xc1, 0xc0, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x10, 0x03, 0x03, 0x01, 0x00, 0x01});
    am.handle(claim, at(3.2));
    EXPECT_EQ(am.state() == AudioMaster::BusState::idle, true);
    EXPECT_EQ(host.distributing, false);
    // BS3000: c0 c1 01 0a 00 00 00 1c 01 02 01
    EXPECT_EQ(sent_contains(host, {0xc0, 0xc1, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x1c, 0x01, 0x02, 0x01}), true);
}

// A room wants N.MUSIC while the video master plays TV on the bus (capture §8, 04:02:32)
TEST(AudioMasterHandover, ClaimBusFromBusyVideoMaster) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);
    auto req = incoming({0xc1, 0x04, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x45, 0x07, 0x01, 0x02, 0x7a, 0x00, 0x02, 0x01, 0x00, 0x00});
    am.handle(req, at(0));
    auto li = incoming({0xc1, 0xc0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x02, 0x01, 0x01});
    am.handle(li, at(0.03));
    host.sent.clear();
    auto busy = incoming({0xc1, 0xc0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x08, 0x05, 0x06, 0x02, 0x01, 0x00, 0x0b, 0x01});
    am.handle(busy, at(0.08));
    // BS3000: c0 c1 01 0b 00 00 00 44 0d 03 02 0b 00 02 01 00 03 01 00 00 00 ff 7a
    EXPECT_EQ(sent_contains(host, {0xc0, 0xc1, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x44, 0x0d, 0x03,
                                   0x02, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0xff, 0x7a}), true);
    EXPECT_EQ(am.state() == AudioMaster::BusState::arbitrating, true);
    host.sent.clear();
    auto notice = incoming({0xc1, 0xc0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x44, 0x0d, 0x07,
                            0x02, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7a});
    am.handle(notice, at(0.25));
    // BS3000: c0 c1 01 0a 00 00 00 10 03 03 01 00 01
    EXPECT_EQ(sent_contains(host, {0xc0, 0xc1, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x10, 0x03, 0x03, 0x01, 0x00, 0x01}), true);
    EXPECT_EQ(am.state() == AudioMaster::BusState::arbitrating, true);
    auto released = incoming({0xc1, 0xc0, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x1c, 0x02, 0x01, 0x01, 0x04});
    am.handle(released, at(0.27));
    EXPECT_EQ(am.state() == AudioMaster::BusState::distributing, true);
    EXPECT_EQ(am.listeners(), (std::set<uint8_t>{0x04}));
    EXPECT_EQ(host.distributing, true);
}

TEST(AudioMasterHandover, ChangeRequestTimesOutIntoClaim) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_participation(false);
    auto req = incoming({0xc1, 0x04, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x45, 0x07, 0x01, 0x02, 0x7a, 0x00, 0x02, 0x01, 0x00, 0x00});
    am.handle(req, at(0));
    auto li = incoming({0xc1, 0xc0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x02, 0x01, 0x01});
    am.handle(li, at(0.03));
    auto busy = incoming({0xc1, 0xc0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x08, 0x05, 0x06, 0x02, 0x01, 0x00, 0x0b, 0x01});
    am.handle(busy, at(0.08));
    am.tick(at(1.2));       // no 0x44 v7: claim anyway
    am.tick(at(2.4));       // no 0x1C: take the bus
    EXPECT_EQ(am.state() == AudioMaster::BusState::distributing, true);
}

// LOCK_MANAGER: we hand the lock over when we hold it, and take it before picking TV (capture §8, 04:02:17)
TEST(AudioMasterLock, TakeLockBeforeVideoSourceAndHandItOver) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_mode(AudioMaster::LockMode::software);
    am.listen_to_video_source(0x0b, at(0));
    ASSERT_TRUE(host.sent.size() == 1);
    // BS3000: 80 c1 01 0b 00 00 00 5c 01 01 01
    EXPECT_EQ(strip(host.sent[0]), (Bytes{0x80, 0xc1, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x01, 0x01}));
    host.sent.clear();
    auto holder = incoming({0xc1, 0x04, 0x01, 0x14, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x02, 0x01});
    am.handle(holder, at(0.02));
    // BS3000: 04 c1 01 0a 00 00 00 5c 01 04 01, then LINK_INFO and LINK_CONNECT_1 to the VM
    ASSERT_TRUE(host.sent.size() == 3);
    EXPECT_EQ(strip(host.sent[0]), (Bytes{0x04, 0xc1, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x04, 0x01}));
    EXPECT_EQ(am.holds_bus_lock(), true);
    host.sent.clear();
    // A room asks for the lock: we answer (BS3000: 04 c1 01 14 00 00 00 5c 01 02 01) and give it up
    auto ask = incoming({0x80, 0x04, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x01, 0x01});
    am.handle(ask, at(10));
    ASSERT_TRUE(host.sent.size() == 1);
    EXPECT_EQ(strip(host.sent[0]), (Bytes{0x04, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x02, 0x01}));
    auto take = incoming({0xc1, 0x04, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x04, 0x01});
    am.handle(take, at(10.02));
    EXPECT_EQ(am.holds_bus_lock(), false);
}

// The holder answers TRANSFER_IMPOSSIBLE (STATUS 3, as BeoIpcML does while a local user still owns the
// key): that is not a grant, so we send no KEY_RECEIVED and wait for the lock timeout
TEST(AudioMasterLock, TransferImpossibleIsNotAGrant) {
    FakeHost host;
    AudioMaster am(host, 6);
    am.set_lock_mode(AudioMaster::LockMode::software);
    am.listen_to_video_source(0x0b, at(0));
    host.sent.clear();
    auto refused = incoming({0xc1, 0x04, 0x01, 0x14, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x03, 0x01});
    am.handle(refused, at(0.02));
    EXPECT_EQ(host.sent.size(), 0u);
    EXPECT_EQ(am.holds_bus_lock(), false);
}

TEST(AudioMasterLock, NobodyHoldsTheLock) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.set_lock_mode(AudioMaster::LockMode::software);
    am.listen_to_video_source(0x0b, at(0));
    am.tick(at(0.6));
    EXPECT_EQ(am.holds_bus_lock(), true);
    EXPECT_EQ(host.sent.size(), 3u);     // lock request, LINK_INFO, LINK_CONNECT_1
}


// Firmware lock (default): the PC2 runs LOCK_MANAGER itself. B&O's sequence on TV: 24 -> IN 44 01 ->
// telegrams -> e4 01 (capture BM5asAM 735.844-736.906).
TEST(AudioMasterLock, FirmwareLockRequestGrantRelease) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.listen_to_video_source(0x0b, at(0));
    EXPECT_EQ(host.lock_requests, 1);
    EXPECT_EQ(host.sent.size(), 0u);           // nothing on the bus until the lock is granted
    am.bus_lock_granted();
    EXPECT_EQ(host.sent.size(), 2u);           // LINK_INFO + LINK_CONNECT_1 to the video master
    EXPECT_EQ(host.lock_releases, 1);
    EXPECT_EQ(am.holds_bus_lock(), false);
}

TEST(AudioMasterLock, FirmwareLockTimesOut) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    am.all_standby(at(0));
    am.tick(at(1.5));
    EXPECT_EQ(host.sent.size(), 0u);
    am.tick(at(2.1));
    EXPECT_EQ(host.sent.size(), 1u);           // STANDBY_1 to ALL, sent anyway
}

TEST(AudioMasterLock, FirmwareModeIgnoresLockTelegrams) {
    FakeHost host;
    AudioMaster am(host, 6);   // the captures were taken in SystemConfiguration 6
    auto ask = incoming({0x80, 0x04, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x5c, 0x01, 0x01, 0x01});
    am.handle(ask, at(0));
    EXPECT_EQ(host.sent.size(), 0u);
}
