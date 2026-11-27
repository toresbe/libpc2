#include "masterlink/source_center.hpp"

#include <boost/log/trivial.hpp>

using ml::Node;
using ml::TelegramType;
using ml::PayloadType;
using ml::addr;

SourceCenter::SourceCenter(AudioMasterHost &host) : host(host) {}

MasterlinkTelegram SourceCenter::make(uint8_t dest, TelegramType type, PayloadType ptype, uint8_t subtype,
                                      std::vector<uint8_t> payload, uint8_t src_src, uint8_t dest_src,
                                      uint8_t from) {
    MasterlinkTelegram t;
    t.dest_node = dest;
    t.src_node = from;
    t.telegram_type = (MasterlinkTelegram::telegram_types)type;
    t.payload_type = (MasterlinkTelegram::payload_types)ptype;
    t.payload_version = subtype;
    t.payload = payload;
    t.src_src = src_src;
    t.dest_src = dest_src;
    return t;
}

void SourceCenter::send(MasterlinkTelegram tgram) { this->host.send_telegram(tgram); }

// SOURCE_SYSTEM_STATUS to ALL_SOURCE_USERS (src_src = the source): active / stopped / idle forms, all
// byte for byte as captured from the BM5
void SourceCenter::send_source_status(uint8_t src, std::vector<uint8_t> payload) {
    send(make(addr(Node::all_source_users), TelegramType::status, PayloadType::source_status,
              (uint8_t)ml::source_status::Subtype::system_status, payload, src));
}

void SourceCenter::send_status_active(uint8_t src) {
    send_source_status(src, ml::source_status::system_status(src, ml::source_status::State::playing));
}

void SourceCenter::send_status_stopped(uint8_t src) {
    send_source_status(src, ml::source_status::stopped(src));
}

void SourceCenter::send_status_idle() {
    send_source_status(ml::source::pc, ml::source_status::system_status(ml::source::pc, ml::source_status::State::idle));
}

void SourceCenter::send_remote_key(uint8_t dest, uint8_t key, uint8_t from) {
    std::scoped_lock lk(this->mutex);
    send(make(dest, TelegramType::command, PayloadType::ir_simul_low, ml::ir_simul::subtype, ml::ir_simul::key(key),
              ml::source::pc, 0x00, from));
}

void SourceCenter::start(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    (void)now;
    send_status_idle();
}

void SourceCenter::tick(clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    if (this->idle_status_pending && now >= this->idle_status_due) {
        this->idle_status_pending = false;
        send_status_idle();
    }
}

void SourceCenter::set_local_playback(bool on) { std::scoped_lock lk(this->mutex); this->local_playback = on; }
uint8_t SourceCenter::current_source() { std::scoped_lock lk(this->mutex); return this->source; }
bool SourceCenter::distributing() { std::scoped_lock lk(this->mutex); return this->source != 0; }

// Observed (live captures): "all standby" (STANDBY_1 to ALL) -> the stopped SOURCE_STATUS, then idle;
// STANDBY_0 for our source -> idle SOURCE_STATUS ~2 s later (unless asked for another source
// meanwhile, or something plays locally); STANDBY_1 for one source -> nothing announced.
void SourceCenter::stop(Announce announce, clock::time_point now) {
    if (!this->source) return;
    if (announce == Announce::stopped)
        send_status_stopped(this->source);
    this->host.set_ml_distribution(false);
    this->host.stop_source();
    this->source = 0;
    if (announce == Announce::idle_later && !this->local_playback) {
        this->idle_status_pending = true;
        this->idle_status_due = now + idle_status_delay;
    }
    if (announce == Announce::stopped)
        send_status_idle();     // observed ~200 ms after the stopped status
}

void SourceCenter::handle(MasterlinkTelegram &t, clock::time_point now) {
    std::scoped_lock lk(this->mutex);
    const auto type = (TelegramType)t.telegram_type;
    const auto pt = (PayloadType)t.payload_type;
    const bool to_us = (t.dest_node == addr(Node::source_center));

    // REQUEST SOURCE_CONNECT, dest_src = source: distribute it
    if (type == TelegramType::request && pt == PayloadType::source_connect && to_us) {
        uint8_t src = t.dest_src;
        if (src != this->source && !this->host.start_source(src)) {
            BOOST_LOG_TRIVIAL(warning) << "Source center: host refused source " << (int)src;
            return;     // no answer observed for this case; stay silent
        }
        this->idle_status_pending = false;
        if (this->source && this->source != src)
            this->host.stop_source();
        this->source = src;
        // As captured: STATUS SOURCE_CONNECT "connected" to the requester, our SOURCE_STATUS to all
        // link devices, PRI_SYSTEM_STATUS to the requester (which relays it to the room)
        send(make(t.src_node, TelegramType::status, PayloadType::source_connect, ml::source_connect::subtype_status,
                  {ml::source_connect::connected}, src));
        this->host.set_ml_distribution(true);
        send_status_active(src);
        send(make(t.src_node, TelegramType::status, PayloadType::pri_system_status, ml::pri_system::subtype_audio,
                  ml::pri_system::playing(src)));
        return;
    }

    if (type != TelegramType::command) return;   // only commands below

    // STANDBY_1 to ALL (subtype_1_all, empty): everything off
    if (pt == PayloadType::standby_1 && t.dest_node == addr(Node::all)) {
        stop(Announce::stopped, now);
        this->host.link_standby();
        return;
    }
    if (!to_us) return;

    switch (pt) {
        case PayloadType::standby_0:     // stop this source (dest_src)
            if (t.dest_src == this->source || t.dest_src == 0)
                stop(Announce::idle_later, now);
            return;
        case PayloadType::standby_1:     // release this source (dest_src)
            if (t.dest_src == this->source || t.dest_src == 0)
                stop(Announce::nothing, now);
            return;
        case PayloadType::source_control:     // "<source> <Beo4 key>"
            if (t.payload.size() >= 2)
                this->host.source_key(t.payload[0], t.payload[1]);
            return;
        default:
            BOOST_LOG_TRIVIAL(debug) << "Source center: unhandled command, payload type " << (int)pt;
            return;
    }
}
