#include "masterlink/audio_master.hpp"

#include <boost/log/trivial.hpp>

std::tm AudioMasterHost::local_time() {
    std::time_t t = std::time(nullptr);
    return *std::localtime(&t);
}

AudioMaster::AudioMaster(AudioMasterHost &host) : host(host) {}

// ---------------------------------------------------------------- telegram builders

MasterlinkTelegram AudioMaster::make(uint8_t dest, uint8_t src, uint8_t type, uint8_t ptype, uint8_t subtype,
                                     std::vector<uint8_t> payload, uint8_t src_src, uint8_t dest_src) {
    MasterlinkTelegram t;
    t.dest_node = dest;
    t.src_node = src;
    t.telegram_type = (MasterlinkTelegram::telegram_types)type;
    t.payload_type = (MasterlinkTelegram::payload_types)ptype;
    t.payload_version = subtype;
    t.payload = payload;
    t.src_src = src_src;
    t.dest_src = dest_src;
    return t;
}

void AudioMaster::send(MasterlinkTelegram tgram) {
    this->host.send_telegram(tgram);
}

// STATUS LINK_INFO v4 "01 01 01": "audio master present"
void AudioMaster::send_link_info_reply(uint8_t to) {
    send(make(to, addr_audio_master, 0x14, pt_link_info, 4, {0x01, 0x01, 0x01}));
}

// STATUS AUDIO_BUS: v4 empty = bus not used by us; v6 "02 01 00 <src>" = we distribute <src>
void AudioMaster::send_audio_bus_status(uint8_t to) {
    if (this->bus_state == BusState::distributing)
        send(make(to, addr_audio_master, 0x14, pt_audio_bus, 6, {0x02, 0x01, 0x00, this->source}));
    else
        send(make(to, addr_audio_master, 0x14, pt_audio_bus, 4, {}));
}

// STATUS SOURCE_STATUS (subtype 4, SOURCE_SYSTEM_STATUS) to ALL_LINK, as sent when distribution starts
void AudioMaster::send_source_status_active(uint8_t src) {
    send(make(addr_all_link, addr_audio_master, 0x14, pt_source_status, 4,
              {src, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x02, 0x01, 0x00, 0x03, 0x01,
               0x01, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
              src));
}

// The "stopped" form, sent when everything is switched off while distributing
void AudioMaster::send_source_status_stopped(uint8_t src) {
    send(make(addr_all_link, addr_audio_master, 0x14, pt_source_status, 4,
              {src, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0xff, 0xff, 0xff, 0xff, 0x06, 0xff, 0xff, 0xff, 0xff,
               0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x01},
              src));
}

// Idle: our PC source (from the PC_1 address), not playing
void AudioMaster::send_source_status_idle() {
    send(make(addr_all_link, addr_pc_1, 0x14, pt_source_status, 4,
              {src_pc, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x01, 0x00, 0x03, 0x01,
               0x01, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00},
              src_pc));
}

// The two replies a room gets once it is connected to our source
void AudioMaster::send_connect_replies(uint8_t to, uint8_t src) {
    send(make(to, addr_audio_master, 0x14, pt_link_connect_status, 5,
              {0x02, src, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00}));
    send(make(to, addr_audio_master, 0x14, pt_pri_system, 1,
              {0x06, src, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}));
}

static uint8_t bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

// STATUS PROGRAMMING_STATUS (clock) v11 to ALL: 0a 00 03 hh mm ss 00 DD MM YY 02 (BCD)
void AudioMaster::send_clock() {
    std::tm t = this->host.local_time();
    send(make(addr_all, addr_audio_master, 0x14, pt_clock, 11,
              {0x0a, 0x00, 0x03, bcd(t.tm_hour), bcd(t.tm_min), bcd(t.tm_sec), 0x00,
               bcd(t.tm_mday), bcd(t.tm_mon + 1), bcd(t.tm_year % 100), 0x02}));
}

// ---------------------------------------------------------------- public API

void AudioMaster::start(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    send_source_status_idle();
    send_clock();
    // Probe for a video master (REQUEST LINK_INFO v4 "01 01 00"); its answer is just noted.
    send(make(addr_video_master, addr_audio_master, 0x0b, pt_link_info, 4, {0x01, 0x01, 0x00}));
    this->next_clock = now + clock_interval;
    this->started = true;
}

void AudioMaster::tick(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    if (this->started && now >= this->next_clock) {
        send_clock();
        this->next_clock = now + clock_interval;
    }
    if (this->bus_state == BusState::arbitrating && now >= this->arbitration_deadline) {
        // No (more) answers from a video master: assume there is none and take the bus.
        BOOST_LOG_TRIVIAL(info) << "No reply from video master; taking the audio bus";
        start_distribution();
    }
}

void AudioMaster::set_local_source(uint8_t src) {
    std::scoped_lock lk(this->mutex);
    this->local_source = src;
    if (this->bus_state == BusState::distributing && src && src != this->source)
        change_source(src);
}

void AudioMaster::listen_to_video_source(uint8_t src) {
    std::scoped_lock lk(this->mutex);
    if (this->bus_state == BusState::distributing)
        stop_distribution(false);
    this->vm_source = src;
    // As captured: LINK_INFO to the VM, then LINK_CONNECT_COMMAND_1 for the source. The VM then
    // asks us LINK_INFO and AUDIO_BUS (answered in handle()) and confirms with LINK_CONNECT_STATUS.
    send(make(addr_video_master, addr_audio_master, 0x0b, pt_link_info, 4, {0x01, 0x01, 0x00}));
    send(make(addr_video_master, addr_audio_master, 0x0b, pt_link_connect, 1,
              {0x02, src, 0x00, 0x02, 0x01, 0x00, 0x00}));
}

void AudioMaster::local_standby() {
    std::scoped_lock lk(this->mutex);
    this->local_source = 0;
    if (this->bus_state == BusState::listening_to_vm || this->vm_source)
        stop_listening_to_vm(true);
    // Rooms still listening keep getting our source (observed: "I power off the BS5 locally;
    // CD continues to play on both Beolink and Beoport").
    if (this->bus_state == BusState::distributing && this->listener_nodes.empty())
        stop_distribution(false);
}

void AudioMaster::all_standby() {
    std::scoped_lock lk(this->mutex);
    send(make(addr_all, addr_audio_master, 0x0a, pt_standby_1, 1, {}));
    if (this->bus_state == BusState::listening_to_vm)
        stop_listening_to_vm(false);
    if (this->bus_state == BusState::distributing || this->bus_state == BusState::arbitrating)
        stop_distribution(true);
    this->local_source = 0;
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
        // We were playing the VM's source: take the bus back first (STANDBY_0 "01 00 01").
        send(make(addr_video_master, addr_audio_master, 0x0a, pt_standby_0, 3, {0x01, 0x00, 0x01}));
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
    send(make(addr_video_master, addr_audio_master, 0x0b, pt_link_info, 4, {0x01, 0x01, 0x00}));
}

void AudioMaster::arbitration_step_link_info_ok(clock::time_point now) {
    this->arbitration = ArbitrationStep::audio_bus;
    this->arbitration_deadline = now + vm_reply_timeout;
    send(make(addr_video_master, addr_audio_master, 0x0b, pt_audio_bus, 1, {}));
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
    // LINK_CONNECT_STATUS v7 "02 <old> 00 02 01 00 02 00 00 00 00 00 <new>" to each listener
    for (auto node : this->listener_nodes) {
        if (old_src)
            send(make(node, addr_audio_master, 0x14, pt_link_connect_status, 7,
                      {0x02, old_src, 0x00, 0x02, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, new_src}));
    }
    this->source = new_src;
    send_source_status_active(new_src);
    for (auto node : this->listener_nodes)
        send_connect_replies(node, new_src);
}

void AudioMaster::stop_listening_to_vm(bool tell_vm) {
    if (tell_vm) {
        // As captured on standby while on TV: LINK_INFO, then STANDBY_1 v2 "01 00" to the VM
        send(make(addr_video_master, addr_audio_master, 0x0b, pt_link_info, 4, {0x01, 0x01, 0x00}));
        send(make(addr_video_master, addr_audio_master, 0x0a, pt_standby_1, 2, {0x01, 0x00}));
    }
    this->host.set_ml_listening(false);
    this->vm_source = 0;
    if (this->bus_state == BusState::listening_to_vm)
        this->bus_state = BusState::idle;
}

// ---------------------------------------------------------------- incoming

void AudioMaster::handle(MasterlinkTelegram &t, clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    const uint8_t type = t.telegram_type;
    const uint8_t pt = t.payload_type;
    const uint8_t from = t.src_node;
    const bool to_us = (t.dest_node == addr_audio_master);
    const bool from_vm = (from == addr_video_master);

    if (type == 0x0b) {     // REQUEST
        if (!to_us) return;
        switch (pt) {
            case pt_link_info:
                send_link_info_reply(from);
                return;
            case pt_audio_bus:
                send_audio_bus_status(from);
                return;
            case pt_local_source:
                // v6 "08 <src>" when we play something locally without distributing it; else v4 empty
                if (this->local_source && this->bus_state != BusState::distributing)
                    send(make(from, addr_audio_master, 0x14, pt_local_source, 6, {0x08, this->local_source}));
                else
                    send(make(from, addr_audio_master, 0x14, pt_local_source, 4, {}));
                return;
            case pt_link_connect:
                if (t.payload.size() >= 2)
                    connect_request(from, t.payload[1], now);
                return;
            default:
                BOOST_LOG_TRIVIAL(debug) << "Unhandled Masterlink request, payload type " << (int)pt;
                return;
        }
    }

    if (type == 0x14 && from_vm && to_us) {     // STATUS from the video master
        if (pt == pt_link_info && this->bus_state == BusState::arbitrating
                && this->arbitration == ArbitrationStep::link_info) {
            arbitration_step_link_info_ok(now);
        } else if (pt == pt_audio_bus && this->bus_state == BusState::arbitrating
                && this->arbitration == ArbitrationStep::audio_bus) {
            if (t.payload.empty()) {
                start_distribution();       // v4 empty: the VM is not using the bus
            } else {
                // The VM distributes something: claim the bus (STANDBY_0 "01 00 01"), then take it
                BOOST_LOG_TRIVIAL(info) << "Video master is using the audio bus; asking it to release";
                send(make(addr_video_master, addr_audio_master, 0x0a, pt_standby_0, 3, {0x01, 0x00, 0x01}));
                start_distribution();
            }
        } else if (pt == pt_link_connect_status && this->vm_source) {
            // VM confirms the source we asked for: play it from the bus
            this->bus_state = BusState::listening_to_vm;
            this->host.set_ml_listening(true);
        }
        return;
    }

    if (type == 0x0a) {     // COMMAND
        if (pt == pt_standby_1) {
            if (t.dest_node == addr_all || t.payload.empty()) {
                // "All standby" (v1, to ALL)
                if (this->bus_state == BusState::listening_to_vm)
                    stop_listening_to_vm(false);
                if (this->bus_state == BusState::distributing || this->bus_state == BusState::arbitrating)
                    stop_distribution(true);
                this->local_source = 0;
                this->host.link_standby();
            } else if (to_us) {
                // One room leaves (v2 "01 00")
                this->listener_nodes.erase(from);
                this->pending_requesters.erase(from);
                if (this->bus_state == BusState::distributing && this->listener_nodes.empty()
                        && !this->local_source)
                    stop_distribution(false);
            }
            return;
        }
        if (pt == pt_standby_0 && from_vm && to_us) {
            // The video master takes the audio bus ("01 00 01")
            if (this->bus_state == BusState::distributing || this->bus_state == BusState::arbitrating)
                stop_distribution(false);
            return;
        }
        if (pt == pt_source_control && to_us && t.payload.size() >= 2) {
            this->host.source_key(t.payload[0], t.payload[1]);
            return;
        }
    }
}
