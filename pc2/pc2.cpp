#include <cstdio>
#include <iomanip>

#include <iostream>
#include <string>
#include <boost/log/trivial.hpp>
#include <vector>
#include <array>
#include <semaphore.h>
#include <functional>

#include "pc2/pc2.hpp"
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
    return true;
}

void PC2Beolink::send_beo4_code(uint8_t dest, uint8_t code) {
    this->pc2->device->send_message({ 0x12, 0xe0, dest, 0xc1, 0x01, 0x0a, 0x00, 0x47, 0x00, 0x20, 0x05, 0x02, 0x00, 0x01, 0xff, 0xff, code, 0x8a, 0x00 });
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

void PC2Device::process_message(const PC2Telegram & tgram) {
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
        this->device->process_message(this->device->inbox->pop_sync());
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
