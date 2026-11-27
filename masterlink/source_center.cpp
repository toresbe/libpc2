#include "masterlink/source_center.hpp"

#include <boost/log/trivial.hpp>

SourceCenter::SourceCenter(AudioMasterHost &host) : host(host) {}

MasterlinkTelegram SourceCenter::make(uint8_t dest, uint8_t type, uint8_t ptype, uint8_t subtype,
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

// SOURCE_STATUS (subtype 4) to ALL_LINK, active / stopped / idle forms, all as captured from the BM5
void SourceCenter::send_status_active(uint8_t src) {
    send(make(AudioMaster::addr_all_link, 0x14, AudioMaster::pt_source_status, 4,
              {src, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x02, 0x01, 0x00, 0x03, 0x01,
               0x01, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, src));
}

void SourceCenter::send_status_stopped(uint8_t src) {
    send(make(AudioMaster::addr_all_link, 0x14, AudioMaster::pt_source_status, 4,
              {src, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0xff, 0xff, 0xff, 0xff, 0x06, 0xff, 0xff, 0xff, 0xff,
               0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x01}, src));
}

void SourceCenter::send_status_idle() {
    send(make(AudioMaster::addr_all_link, 0x14, AudioMaster::pt_source_status, 4,
              {src_pc, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x01, 0x00, 0x03, 0x01,
               0x01, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, src_pc));
}

void SourceCenter::send_remote_key(uint8_t dest, uint8_t key, uint8_t from) {
    std::scoped_lock lk(this->mutex);
    send(make(dest, 0x0a, pt_ir_simul, 2, {0x00, 0x01, 0xff, 0xff, key}, src_pc, 0x00, from));
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
    const uint8_t type = t.telegram_type;
    const uint8_t pt = t.payload_type;
    const bool to_us = (t.dest_node == addr_source_center);

    // REQUEST SOURCE_CONNECT, dest_src = source: distribute it
    if (type == 0x0b && pt == pt_source_connect && to_us) {
        uint8_t src = t.dest_src;
        if (src != this->source && !this->host.start_source(src)) {
            BOOST_LOG_TRIVIAL(warning) << "Source center: host refused source " << (int)src;
            return;     // no answer observed for this case; stay silent
        }
        this->idle_status_pending = false;
        if (this->source && this->source != src)
            this->host.stop_source();
        this->source = src;
        // As captured: STATUS SOURCE_CONNECT v8 "01" to the requester, our SOURCE_STATUS to all
        // link devices, PRI_SYSTEM_STATUS v1 to the requester (which relays it to the room)
        send(make(t.src_node, 0x14, pt_source_connect, 8, {0x01}, src));
        this->host.set_ml_distribution(true);
        send_status_active(src);
        send(make(t.src_node, 0x14, AudioMaster::pt_pri_system, 1,
                  {0x06, src, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}));
        return;
    }

    if (type != 0x0a) return;   // only commands below

    // STANDBY_1 to ALL (v1, empty): everything off
    if (pt == AudioMaster::pt_standby_1 && t.dest_node == AudioMaster::addr_all) {
        stop(Announce::stopped, now);
        this->host.link_standby();
        return;
    }
    if (!to_us) return;

    switch (pt) {
        case AudioMaster::pt_standby_0:     // stop this source (dest_src)
            if (t.dest_src == this->source || t.dest_src == 0)
                stop(Announce::idle_later, now);
            return;
        case AudioMaster::pt_standby_1:     // release this source (dest_src)
            if (t.dest_src == this->source || t.dest_src == 0)
                stop(Announce::nothing, now);
            return;
        case AudioMaster::pt_source_control:
            if (t.payload.size() >= 2)
                this->host.source_key(t.payload[0], t.payload[1]);
            return;
        default:
            BOOST_LOG_TRIVIAL(debug) << "Source center: unhandled command, payload type " << (int)pt;
            return;
    }
}
