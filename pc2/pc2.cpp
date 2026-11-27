#include <cstdio>
#include <iomanip>

#include <iostream>
#include <string>
#include <boost/log/trivial.hpp>
#include <vector>
#include <array>
#include <functional>

#include "pc2/pc2.hpp"
#include "masterlink/audio_master.hpp"
#include "masterlink/source_center.hpp"
#include "pc2/pc2device.hpp"
#include "pc2/mixer.hpp"
#include "pc2/beo4.hpp"

void PC2Beolink::send_telegram(MasterlinkTelegram &tgram) {
    PC2Message msg = tgram.serialize();
    msg.insert(msg.begin(), 0xe0);
    this->pc2->device->send_message(msg);
}

PC2::PC2(PC2Interface * interface) {
    this->interface = interface;
    this->interface->pc2 = this;
    this->keystroke_callback = nullptr;

    this->device = new PC2Device(this);
    this->beolink = new PC2Beolink(this);
    this->mixer = new PC2Mixer(this);
}

bool PC2::open() {
    this->device->open();
    this->device->init();
    if (this->interface->address_mask == PC2Interface::address_mask_t::audio_master)
        this->beolink->start_audio_master();
    else if (this->interface->address_mask == PC2Interface::address_mask_t::source_center)
        this->beolink->start_source_center();
    return true;
}

/**! \brief Connects the AudioMaster state machine to this PC2: telegrams, mixer and interface hooks
 */
class PC2AudioMasterHost : public AudioMasterHost {
    PC2 *pc2;
    public:
    PC2AudioMasterHost(PC2 *pc2) : pc2(pc2) {}
    void send_telegram(MasterlinkTelegram &tgram) override { this->pc2->beolink->send_telegram(tgram); }
    void set_ml_distribution(bool on) override { this->pc2->mixer->ml_distribute(on); }
    void set_ml_listening(bool on) override { this->pc2->mixer->transmit_from_ml(on); }
    bool start_source(uint8_t src) override { return this->pc2->interface->ml_start_source(src); }
    void stop_source() override { this->pc2->interface->ml_stop_source(); }
    void source_key(uint8_t src, uint8_t key) override { this->pc2->interface->ml_source_key(src, key); }
    void link_standby() override { this->pc2->interface->ml_link_standby(); }
};

void PC2Beolink::start_audio_master() {
    if (this->audio_master) return;
    if (!this->audio_master_host) this->audio_master_host = new PC2AudioMasterHost(this->pc2);
    this->audio_master = new AudioMaster(*this->audio_master_host);
    this->audio_master->start();
    this->start_ticker();
}

void PC2Beolink::start_source_center() {
    if (this->source_center) return;
    if (!this->audio_master_host) this->audio_master_host = new PC2AudioMasterHost(this->pc2);
    this->source_center = new SourceCenter(*this->audio_master_host);
    this->source_center->start();
    this->start_ticker();
}

// Timeouts, delayed announcements and the periodic clock broadcast
void PC2Beolink::start_ticker() {
    if (this->ticker) return;
    this->ticking = true;
    this->ticker = new std::thread([this] {
        while (this->ticking) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (this->audio_master) this->audio_master->tick();
            if (this->source_center) this->source_center->tick();
        }
    });
}

PC2Beolink::~PC2Beolink() {
    if (this->ticker) {
        this->ticking = false;
        this->ticker->join();
        delete this->ticker;
    }
    delete this->audio_master;
    delete this->source_center;
    delete this->audio_master_host;
}

/** \brief Press a Beo4 key on another node (COMMAND IR_SIMUL_LOW, 0x20), as the BeoMaster 5 panel does
 *  towards the room's master. Captured: dest c1 01 0a 00 47 00 20 05 02 | 00 01 ff ff <key>.
 *  (The old hand-written version prefixed a stray 0x12 and had a fixed checksum.)
 */
void PC2Beolink::send_beo4_code(uint8_t dest, uint8_t code) {
    MasterlinkTelegram key;
    key.dest_node = dest;
    key.src_node = 0xc1;
    key.telegram_type = MasterlinkTelegram::telegram_types::command;
    key.src_src = 0x47;
    key.payload_type = (MasterlinkTelegram::payload_types)0x20;
    key.payload_version = 2;
    key.payload = { 0x00, 0x01, 0xff, 0xff, code };
    this->send_telegram(key);
};

void PC2Beolink::request_source(uint8_t source_id) {
    BOOST_LOG_TRIVIAL(error) << "Dummy legacy function invoked to request source " << (unsigned int) source_id;
}

PC2Beolink::PC2Beolink(PC2 *pc2) {
    this->pc2 = pc2;
}

void PC2Beolink::process_beo4_keycode(uint8_t type, uint8_t keycode) {
    // TODO: This should offer a more semantically meaningfully separated callback structure.

    BOOST_LOG_TRIVIAL(debug) << "Got remote control code " << std::hex << std::setw(2) << std::setfill('0') << (short unsigned int) keycode;

    if(type == 0x0F) {
       if(keycode == Beo4::keycode::standby) {
           if (this->audio_master)
               this->audio_master->all_standby();
           else
               this->pc2->beolink->send_shutdown_all();
       }
    // TODO: Keycodes need to be separated into categories here; is this something 
    // we need to pass along to the source, or is it for libpc2?
    } else { 
        if (this->pc2->keystroke_callback) {
            this->pc2->keystroke_callback((Beo4::keycode)keycode);
        } else {
            BOOST_LOG_TRIVIAL(error) << "Not calling Beo4 callback; none registered!";
        }
    }
}

void PC2Device::process_message(const PC2Message & tgram) {
    if (tgram[2] == 0x00) {
        this->pc2->beolink->process_ml_telegram(tgram);
    }
    if (tgram[2] == 0x02) {
        this->pc2->beolink->process_beo4_keycode(tgram[4], tgram[6]);
    }
    if (tgram[2] == 0x03 || tgram[2] == 0x1D) {
        // PC2 device sending mixer state
        this->pc2->mixer->process_mixer_state(tgram);
    }
    if (tgram[2] == 0x06) {
        this->pc2->mixer->process_headphone_state(tgram);
    }
}

void PC2::event_loop(volatile bool & keepRunning) {
    while(keepRunning) {
        this->device->process_message(this->device->inbox.pop_sync());
    }
}

// TODO: Rewrite to use more current idioms
void PC2Beolink::broadcast_timestamp() {
    uint8_t hour, minute, seconds;
    uint8_t day, month, year;
    std::time_t t = std::time(nullptr);
    char mbstr[100];
    if (std::strftime(mbstr, sizeof(mbstr), "%H%M%S%d%m%y", std::localtime(&t))) {
        std::sscanf(mbstr, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &hour, &minute, &seconds, &day, &month, &year);
    }
    this->pc2->device->send_message({ 0xe0, 0x80, 0xc1, 0x01, 0x14, 0x00, \
            0x00, 0x00, 0x40, 0x0b, 0x0b, 0x0a, \
            0x00, 0x03, hour, minute, seconds,  \
            0x00, day, month, year, 0x02, 0x76, \
            0x00 });
}
