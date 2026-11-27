#include "masterlink/audio_master.hpp"

#include <boost/log/trivial.hpp>

using ml::Node;
using ml::TelegramType;
using ml::PayloadType;
using ml::addr;

std::tm AudioMasterHost::local_time() {
    std::time_t t = std::time(nullptr);
    return *std::localtime(&t);
}

AudioMaster::AudioMaster(AudioMasterHost &host, uint8_t system_configuration)
    : host(host), system_configuration(system_configuration) {}

// ---------------------------------------------------------------- telegram builders

MasterlinkTelegram AudioMaster::make(uint8_t dest, Node src, TelegramType type, PayloadType ptype,
                                     uint8_t subtype, std::vector<uint8_t> payload, uint8_t src_src,
                                     uint8_t dest_src) {
    MasterlinkTelegram t;
    t.dest_node = dest;
    t.src_node = addr(src);
    t.telegram_type = (MasterlinkTelegram::telegram_types)type;
    t.payload_type = (MasterlinkTelegram::payload_types)ptype;
    t.payload_version = subtype;
    t.payload = payload;
    t.src_src = src_src;
    t.dest_src = dest_src;
    return t;
}

MasterlinkTelegram AudioMaster::make(uint8_t dest, TelegramType type, PayloadType ptype, uint8_t subtype,
                                     std::vector<uint8_t> payload) {
    return make(dest, Node::audio_master, type, ptype, subtype, payload);
}

void AudioMaster::send(MasterlinkTelegram tgram) {
    this->host.send_telegram(tgram);
}

// STATUS LINK_INFO: "audio master present"
void AudioMaster::send_link_info_reply(uint8_t to) {
    send(make(to, TelegramType::status, PayloadType::link_info, ml::link_info::subtype,
              ml::link_info::answer(ml::link_info::NodeKind::audio_master, true)));
}

// STATUS AUDIO_BUS: free (empty) or busy with our source
void AudioMaster::send_audio_bus_status(uint8_t to) {
    if (this->bus_state == BusState::distributing)
        send(make(to, TelegramType::status, PayloadType::audio_bus, ml::audio_bus::subtype_busy,
                  ml::audio_bus::busy(this->source)));
    else
        send(make(to, TelegramType::status, PayloadType::audio_bus, ml::audio_bus::subtype_free));
}

// SOURCE_SYSTEM_STATUS to ALL_SOURCE_USERS, as sent when distribution starts
void AudioMaster::send_source_status_active(uint8_t src) {
    send(make(addr(Node::all_source_users), Node::audio_master, TelegramType::status, PayloadType::source_status,
              (uint8_t)ml::source_status::Subtype::system_status,
              ml::source_status::system_status(src, ml::source_status::State::playing), src));
}

// The "stopped" form, sent when everything is switched off while distributing
void AudioMaster::send_source_status_stopped(uint8_t src) {
    send(make(addr(Node::all_source_users), Node::audio_master, TelegramType::status, PayloadType::source_status,
              (uint8_t)ml::source_status::Subtype::system_status, ml::source_status::stopped(src), src));
}

// Idle: our PC source (from the PC_1 address), not playing
void AudioMaster::send_source_status_idle() {
    send(make(addr(Node::all_source_users), Node::pc_1, TelegramType::status, PayloadType::source_status,
              (uint8_t)ml::source_status::Subtype::system_status,
              ml::source_status::system_status(ml::source::pc, ml::source_status::State::idle), ml::source::pc));
}

// The two replies a room gets once it is connected to our source: CONNECTED_SIGNAL and PRI_SYSTEM_STATUS
void AudioMaster::send_connect_replies(uint8_t to, uint8_t src) {
    send(make(to, TelegramType::status, PayloadType::link_connect_0,
              (uint8_t)ml::link_connect::Signal::connected, ml::link_connect::connected(src)));
    send(make(to, TelegramType::status, PayloadType::pri_system_status, ml::pri_system::subtype_audio,
              ml::pri_system::playing(src)));
}

static uint8_t bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

// STATUS PROGRAMMING_STATUS (clock) to ALL: 0a 00 03 hh mm ss 00 DD MM YY <priority> (BCD)
void AudioMaster::send_clock() {
    std::tm t = this->host.local_time();
    send(make(addr(Node::all), TelegramType::status, PayloadType::programming_status, ml::clock::subtype,
              {ml::clock::byte0, ml::clock::byte1, ml::clock::byte2,
               bcd(t.tm_hour), bcd(t.tm_min), bcd(t.tm_sec), ml::clock::byte6,
               bcd(t.tm_mday), bcd(t.tm_mon + 1), bcd(t.tm_year % 100), ml::clock::default_priority}));
}

// REQUEST LINK_INFO to the video master, as the audio master
void AudioMaster::send_link_info_request_to_vm() {
    send(make(addr(Node::video_master), TelegramType::request, PayloadType::link_info, ml::link_info::subtype,
              ml::link_info::request(ml::link_info::NodeKind::audio_master, this->system_configuration)));
}

// COMMAND STANDBY_0 on the audio link: take the audio bus from the video master
void AudioMaster::send_take_bus_from_vm() {
    send(make(addr(Node::video_master), TelegramType::command, PayloadType::standby_0, ml::standby::subtype_0,
              ml::standby::take_audio_bus()));
}

// ---------------------------------------------------------------- public API

void AudioMaster::start(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    send_source_status_idle();
    send_clock();
    // Probe for a video master; its answer is just noted.
    send_link_info_request_to_vm();
    this->next_clock = now + clock_interval;
    this->started = true;
}

void AudioMaster::tick(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    if (this->started && now >= this->next_clock) {
        send_clock();
        this->next_clock = now + clock_interval;
    }
    if (this->lock_pending && now >= this->lock_deadline) {
        // Software: nobody answered, so nobody holds it. Firmware: no IN 44 01 (the PC2 may loop
        // REQUEST/COMMAND v5 forever if nobody answers); act anyway rather than block TV/standby.
        if (this->lock_mode == LockMode::firmware)
            BOOST_LOG_TRIVIAL(warning) << "No bus lock grant (IN 44) from the PC2; going ahead";
        lock_acquired();
    }
    if (this->bus_state == BusState::arbitrating && now >= this->arbitration_deadline) {
        if (this->arbitration == ArbitrationStep::change_request) {
            BOOST_LOG_TRIVIAL(info) << "Video master did not answer the change request; claiming the bus";
            send_take_bus_from_vm();
            this->arbitration = ArbitrationStep::claim;
            this->arbitration_deadline = now + vm_reply_timeout;
        } else {
            // No (more) answers from a video master: assume there is none / it let go, and take the bus.
            BOOST_LOG_TRIVIAL(info) << "No reply from video master; taking the audio bus";
            start_distribution();
        }
    }
}

void AudioMaster::set_lock_mode(LockMode mode) { std::scoped_lock lk(this->mutex); this->lock_mode = mode; }
void AudioMaster::set_lock_participation(bool on) { set_lock_mode(on ? LockMode::software : LockMode::off); }

void AudioMaster::bus_lock_granted() {
    std::scoped_lock lk(this->mutex);
    if (this->lock_mode == LockMode::firmware && this->lock_pending)
        lock_acquired();
}
bool AudioMaster::holds_bus_lock() { std::scoped_lock lk(this->mutex); return this->holds_lock; }

// LOCK_MANAGER: broadcast REQUEST (subtype_request); the current holder answers STATUS (subtype_holder);
// we take it over with COMMAND (subtype_take) to the holder. If nobody answers, nobody holds it.
void AudioMaster::with_lock(std::function<void()> action, clock::time_point now) {
    if (this->lock_mode == LockMode::off || this->holds_lock) { action(); return; }
    this->lock_waiters.push_back(action);
    if (this->lock_pending) return;
    this->lock_pending = true;
    if (this->lock_mode == LockMode::firmware) {
        this->lock_deadline = now + firmware_lock_timeout;
        this->host.request_bus_lock();                                     // 24
    } else {
        this->lock_deadline = now + lock_timeout;
        send(make(addr(Node::all), TelegramType::request, PayloadType::lock_manager,
                  ml::lock_manager::subtype_request, {ml::lock_manager::payload}));
    }
}

void AudioMaster::lock_acquired() {
    this->lock_pending = false;
    this->holds_lock = true;
    auto waiters = std::move(this->lock_waiters);
    this->lock_waiters.clear();
    for (auto &w : waiters) w();
    if (this->lock_mode == LockMode::firmware) {
        // B&O releases right after its telegrams (e4 01) rather than waiting out the firmware's hold
        this->host.release_bus_lock();
        this->holds_lock = false;
    }
}

void AudioMaster::handle_lock(MasterlinkTelegram &t) {
    const auto type = (TelegramType)t.telegram_type;
    const bool to_us = (t.dest_node == addr(Node::audio_master));
    if (type == TelegramType::request && t.dest_node == addr(Node::all)) {
        // Someone wants the lock; answer if we hold it
        if (this->holds_lock && t.src_node != addr(Node::audio_master))
            send(make(t.src_node, TelegramType::status, PayloadType::lock_manager,
                      ml::lock_manager::subtype_holder, {ml::lock_manager::payload}));
    } else if (type == TelegramType::status && to_us && this->lock_pending) {
        // The holder hands it to us
        send(make(t.src_node, TelegramType::command, PayloadType::lock_manager,
                  ml::lock_manager::subtype_take, {ml::lock_manager::payload}));
        lock_acquired();
    } else if (type == TelegramType::command && to_us) {
        // We handed it over
        this->holds_lock = false;
    }
}

void AudioMaster::set_local_source(uint8_t src) {
    std::scoped_lock lk(this->mutex);
    this->local_source = src;
    if (this->bus_state == BusState::distributing && src && src != this->source)
        change_source(src);
}

void AudioMaster::listen_to_video_source(uint8_t src, clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    if (this->bus_state == BusState::distributing)
        stop_distribution(false);
    this->vm_source = src;
    // As captured: (lock), LINK_INFO to the VM, then LINK_CONNECT_COMMAND_1 for the source. The VM then
    // asks us LINK_INFO and AUDIO_BUS (answered in handle()) and confirms with CONNECTED_SIGNAL.
    with_lock([this, src] {
        send_link_info_request_to_vm();
        send(make(addr(Node::video_master), TelegramType::request, PayloadType::link_connect_1,
                  (uint8_t)ml::link_connect::Signal::connect, ml::link_connect::connect_request(src)));
    }, now);
}

void AudioMaster::local_standby(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    this->local_source = 0;
    if (this->bus_state == BusState::listening_to_vm || this->vm_source) {
        // B&O takes the bus lock around releasing the video master's source (24 ... e4 01)
        this->host.set_ml_listening(false);
        with_lock([this] { stop_listening_to_vm(true); }, now);
    }
    // Rooms still listening keep getting our source (observed: "I power off the BS5 locally;
    // CD continues to play on both Beolink and Beoport").
    if (this->bus_state == BusState::distributing && this->listener_nodes.empty())
        stop_distribution(false);
}

void AudioMaster::all_standby(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    with_lock([this] {
        send(make(addr(Node::all), TelegramType::command, PayloadType::standby_1, ml::standby::subtype_1_all));
        if (this->bus_state == BusState::listening_to_vm)
            stop_listening_to_vm(false);
        if (this->bus_state == BusState::distributing || this->bus_state == BusState::arbitrating)
            stop_distribution(true);
        this->local_source = 0;
    }, now);
}

AudioMaster::BusState AudioMaster::state() { std::scoped_lock lk(this->mutex); return this->bus_state; }
uint8_t AudioMaster::current_source() { std::scoped_lock lk(this->mutex); return this->source; }
std::set<uint8_t> AudioMaster::listeners() { std::scoped_lock lk(this->mutex); return this->listener_nodes; }

// ---------------------------------------------------------------- flow

void AudioMaster::connect_request(uint8_t node, uint8_t src, clock::time_point now) {
    if (this->bus_state == BusState::distributing && src == this->source) {
        // Already on the bus: just connect the room.
        this->listener_nodes.insert(node);
        send_connect_replies(node, src);
        return;
    }
    if (this->bus_state == BusState::distributing) {
        // A room picks another of our sources: move everybody over.
        if (!this->host.start_source(src)) {
            BOOST_LOG_TRIVIAL(warning) << "Host refused source " << (int)src;
            return;
        }
        this->listener_nodes.insert(node);
        change_source(src);
        return;
    }
    if (this->bus_state == BusState::arbitrating) {
        this->pending_requesters.insert(node);
        this->source = src;
        return;
    }
    if (!this->host.start_source(src)) {
        BOOST_LOG_TRIVIAL(warning) << "Host refused source " << (int)src;
        return;
    }
    if (this->bus_state == BusState::listening_to_vm) {
        // We were playing the VM's source: take the bus back first (STANDBY_0 on the audio link).
        send_take_bus_from_vm();
        this->host.set_ml_listening(false);
        this->bus_state = BusState::idle;
        this->vm_source = 0;
    }
    this->pending_requesters.insert(node);
    begin_arbitration(src, now);
}

void AudioMaster::begin_arbitration(uint8_t src, clock::time_point now) {
    this->source = src;
    this->bus_state = BusState::arbitrating;
    this->arbitration = ArbitrationStep::link_info;
    this->arbitration_deadline = now + vm_reply_timeout;
    send_link_info_request_to_vm();
}

void AudioMaster::arbitration_step_link_info_ok(clock::time_point now) {
    this->arbitration = ArbitrationStep::audio_bus;
    this->arbitration_deadline = now + vm_reply_timeout;
    send(make(addr(Node::video_master), TelegramType::request, PayloadType::audio_bus,
              ml::audio_bus::subtype_request));
}

void AudioMaster::start_distribution() {
    this->arbitration = ArbitrationStep::none;
    this->bus_state = BusState::distributing;
    send_source_status_active(this->source);
    this->host.set_ml_distribution(true);
    for (auto node : this->pending_requesters) {
        this->listener_nodes.insert(node);
        send_connect_replies(node, this->source);
    }
    this->pending_requesters.clear();
}

void AudioMaster::stop_distribution(bool announce_stopped) {
    uint8_t was = this->source;
    if (announce_stopped && was)
        send_source_status_stopped(was);
    this->host.set_ml_distribution(false);
    this->host.stop_source();
    this->bus_state = BusState::idle;
    this->arbitration = ArbitrationStep::none;
    this->listener_nodes.clear();
    this->pending_requesters.clear();
    this->source = 0;
    send_source_status_idle();
}

void AudioMaster::change_source(uint8_t new_src) {
    uint8_t old_src = this->source;
    // DISCONNECTED_SIGNAL "<old> -> <new>" to each listener
    for (auto node : this->listener_nodes) {
        if (old_src)
            send(make(node, TelegramType::status, PayloadType::link_connect_0,
                      (uint8_t)ml::link_connect::Signal::disconnected, ml::link_connect::changed(old_src, new_src)));
    }
    this->source = new_src;
    send_source_status_active(new_src);
    for (auto node : this->listener_nodes)
        send_connect_replies(node, new_src);
}

void AudioMaster::stop_listening_to_vm(bool tell_vm) {
    if (tell_vm) {
        // As captured on standby while on TV: LINK_INFO, then STANDBY_1 (we leave the audio link) to the VM
        send_link_info_request_to_vm();
        send(make(addr(Node::video_master), TelegramType::command, PayloadType::standby_1,
                  ml::standby::subtype_1_node, ml::standby::leave_audio()));
    }
    this->host.set_ml_listening(false);
    this->vm_source = 0;
    if (this->bus_state == BusState::listening_to_vm)
        this->bus_state = BusState::idle;
}

// ---------------------------------------------------------------- incoming

void AudioMaster::handle(MasterlinkTelegram &t, clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    const auto type = (TelegramType)t.telegram_type;
    const auto pt = (PayloadType)t.payload_type;
    const uint8_t from = t.src_node;
    const bool to_us = (t.dest_node == addr(Node::audio_master));
    const bool from_vm = (from == addr(Node::video_master));
    namespace lc = ml::link_connect;

    if (pt == PayloadType::lock_manager) {
        if (this->lock_mode == LockMode::software)
            handle_lock(t);
        return;
    }

    if (type == TelegramType::request) {
        if (!to_us) return;
        switch (pt) {
            case PayloadType::link_info:
                send_link_info_reply(from);
                return;
            case PayloadType::audio_bus:
                send_audio_bus_status(from);
                return;
            case PayloadType::local_source:
                // "playing <src>" when we play something locally without distributing it; else "none"
                if (this->local_source && this->bus_state != BusState::distributing)
                    send(make(from, TelegramType::status, PayloadType::local_source,
                              ml::local_source::subtype_playing, ml::local_source::playing(this->local_source)));
                else
                    send(make(from, TelegramType::status, PayloadType::local_source, ml::local_source::subtype_none));
                return;
            case PayloadType::link_connect_1:
                // CONNECT: payload byte 1 is the source
                if (t.payload.size() >= 2)
                    connect_request(from, t.payload[1], now);
                return;
            case PayloadType::link_connect_0:
                // DISCONNECT_SIGNAL "<cur> -> <wanted>" (13-byte form, <wanted> last): the video master wants
                // the bus for <wanted>. Answer with DISCONNECTED_SIGNAL and tell our listeners too; it will
                // then claim the bus with STANDBY_0 on the audio link.
                if (t.payload.size() >= 13) {
                    uint8_t cur = this->bus_state == BusState::distributing ? this->source : t.payload[1];
                    uint8_t wanted = t.payload[12];
                    for (auto node : this->listener_nodes)
                        send(make(node, TelegramType::status, PayloadType::link_connect_0,
                                  (uint8_t)lc::Signal::disconnected, lc::changed(cur, wanted)));
                    send(make(from, TelegramType::status, PayloadType::link_connect_0, (uint8_t)lc::Signal::disconnected,
                              lc::with_second_source(cur, lc::Mode::with_wanted_source, true,
                                                     lc::wanted_byte11_answer, wanted)));
                }
                return;
            default:
                BOOST_LOG_TRIVIAL(debug) << "Unhandled Masterlink request, payload type " << (int)pt;
                return;
        }
    }

    if (type == TelegramType::status && from_vm && to_us) {     // STATUS from the video master
        if (pt == PayloadType::link_info && this->bus_state == BusState::arbitrating
                && this->arbitration == ArbitrationStep::link_info) {
            arbitration_step_link_info_ok(now);
        } else if (pt == PayloadType::audio_bus && this->bus_state == BusState::arbitrating
                && this->arbitration == ArbitrationStep::audio_bus) {
            if (t.payload.empty()) {
                start_distribution();       // free: the VM is not using the bus
            } else {
                // busy "02 01 00 <src> [01]": the VM distributes <src>. Ask it to switch to ours
                // (DISCONNECT_SIGNAL <src> -> ours).
                this->vm_busy_source = t.payload.size() >= 4 ? t.payload[3] : 0;
                BOOST_LOG_TRIVIAL(info) << "Video master is using the audio bus; requesting a change";
                send(make(addr(Node::video_master), TelegramType::request, PayloadType::link_connect_0,
                          (uint8_t)lc::Signal::disconnect,
                          lc::with_second_source(this->vm_busy_source, lc::Mode::with_wanted_source, true,
                                                 lc::wanted_byte11_audio_master, this->source)));
                this->arbitration = ArbitrationStep::change_request;
                this->arbitration_deadline = now + vm_reply_timeout;
            }
        } else if (pt == PayloadType::link_connect_0 && t.payload_version == (uint8_t)lc::Signal::disconnected
                && this->bus_state == BusState::arbitrating && this->arbitration == ArbitrationStep::change_request) {
            // The VM agreed (DISCONNECTED_SIGNAL): claim the bus
            send_take_bus_from_vm();
            this->arbitration = ArbitrationStep::claim;
            this->arbitration_deadline = now + vm_reply_timeout;
        } else if (pt == PayloadType::link_connect_0 && this->vm_source) {
            // VM confirms the source we asked for: play it from the bus
            this->bus_state = BusState::listening_to_vm;
            this->host.set_ml_listening(true);
        }
        return;
    }

    if (type == TelegramType::command) {
        if (pt == PayloadType::standby_1) {
            if (t.dest_node == addr(Node::all) || t.payload.empty()) {
                // "All standby" (subtype_1_all, to ALL)
                if (this->bus_state == BusState::listening_to_vm)
                    stop_listening_to_vm(false);
                if (this->bus_state == BusState::distributing || this->bus_state == BusState::arbitrating)
                    stop_distribution(true);
                this->local_source = 0;
                this->host.link_standby();
            } else if (to_us) {
                // One room leaves (subtype_1_node, "<audio link> <video link>")
                this->listener_nodes.erase(from);
                this->pending_requesters.erase(from);
                if (this->bus_state == BusState::distributing && this->listener_nodes.empty()
                        && !this->local_source)
                    stop_distribution(false);
            }
            return;
        }
        if (pt == PayloadType::standby_0 && from_vm && to_us) {
            // The video master takes the audio bus; confirm with COMMAND 0x1C (bus released)
            if (this->bus_state == BusState::distributing || this->bus_state == BusState::arbitrating) {
                stop_distribution(false);
                send(make(addr(Node::video_master), TelegramType::command, PayloadType::bus_released,
                          ml::bus_released::subtype_bs3000, {ml::bus_released::byte0}));
            }
            return;
        }
        if (pt == PayloadType::bus_released && from_vm && to_us) {
            // The VM confirms it let go of the bus
            if (this->bus_state == BusState::arbitrating && this->arbitration == ArbitrationStep::claim)
                start_distribution();
            return;
        }
        if (pt == PayloadType::source_control && to_us && t.payload.size() >= 2) {
            // "<source> <Beo4 key>"
            this->host.source_key(t.payload[0], t.payload[1]);
            return;
        }
    }
}
