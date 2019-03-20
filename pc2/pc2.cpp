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
    sem_init(&this->semaphore, true, 0);
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
//    this->pc2->device->send_message({ 0x24 }); // don't know what this does, only the PC2 uses this
//    this->device->get_data(); // expect 24 01
//    this->pc2->device->send_message({0xe0, 0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 
//            0x04, 0x03, 0x04, 0x0a, 0x01, 0x00, 0xe4, 0x00});
//    expect_ack();
//    this->pc2->device->send_message({ 0xe0, 0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 
//            0x45, 0x07, 0x01, 0x02, source_id, 0x00, 0x02, 0x01, 
//            0x00, 0x00, 0xad, 0x00, 0x61 });
//    expect_ack();
//    this->device->get_data(); // expect 0x44 back
//    this->pc2->device->send_message({ 0xe4, 0x01 });
//    expect_ack();
}

PC2Beolink::PC2Beolink(PC2 *pc2) {
    this->pc2 = pc2;
}

void PC2Beolink::process_beo4_keycode(uint8_t type, uint8_t keycode) {
    // TODO: This should offer a more semantically meaningfully separated callback structure.

    BOOST_LOG_TRIVIAL(debug) << "Got remote control code " << std::hex << std::setw(2) << std::setfill('0') << (short unsigned int) keycode;

    if(type == 0x0F && keycode == 0x0C) {
        BOOST_LOG_TRIVIAL(warning) << "We should send a Masterlink shutdown signal now!";
    }

    // TODO: Keycodes need to be separated into classes here.
    if (this->pc2->keystroke_callback) {
        this->pc2->keystroke_callback((Beo4::keycode)keycode);
    } else {
        BOOST_LOG_TRIVIAL(error) << "Not calling Beo4 callback; none registered!";
    }
}

void PC2Device::process_message(PC2Telegram & tgram) {
    if (tgram[2] == 0x00) {
        this->pc2->beolink->process_ml_telegram(tgram);
    }
    if (tgram[2] == 0x02) {
        this->pc2->beolink->process_beo4_keycode(tgram[4], tgram[6]);
    }
    if (tgram[2] == 0x03) {
        // PC2 device sending mixer state
        this->pc2->mixer->process_mixer_state(tgram);
    }
    if (tgram[2] == 0x06) {
        this->pc2->mixer->process_headphone_state(tgram);
    }
}


/*
//void PC2::send_audio() {
//    PC2Telegram telegram;
//    this->device->write({ 0xfa, 0x38, 0xf0, 0x88, 0x40, 0x00, 0x00 });
//    telegram = this->device->get_data();
//    // This is a source status (V. MASTER also sends a 0x87 telegram.) Why is it sending source 0x7A (A.MEM2)?
//    BOOST_LOG_TRIVIAL(debug) << "Sending source status";
//    this->device->write({ 0xe0,0x83,0xc1,0x01,0x14,0x00,0x7a,0x00,0x87,0x1e,0x04,0x7a,0x02,0x00,0x00,0x40, \
//            0x28,0x01,0x00,0x00,0x00,0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, \
//            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x62,0x00,0x00 });
//    expect_ack();
//    BOOST_LOG_TRIVIAL(debug) << "Pinging Beosystem 3";
//    this->device->write({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x01,0x01,0xa4,0x00 });
//    telegram = this->device->get_data(); // expect an ACK
//    telegram = this->device->get_data(); // expect reply from V MASTER
//    this->device->write({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00 }); // Telegram to V. MASTER: Plz be quiet
//    telegram = this->device->get_data(); // expect an ACK
//    telegram = this->device->get_data(); // expect reply from V MASTER
//    this->device->write({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x08,0x00,0x01,0x96,0x00 });
//    telegram = this->device->get_data(); // expect an ACK
//    telegram = this->device->get_data(); // expect reply from V MASTER
//    this->mixer->speaker_power(true);
//    telegram = this->device->get_data(); // not sure what this is
//    this->device->write({ 0xfa, 0x30, 0xd0, 0x65, 0x80, 0x6c, 0x22 });
//    this->device->write({ 0xf9, 0x64 });
//    this->mixer->set_parameters(0x22, 0, 0, 0, false);
//    this->mixer->ml_distribute(true);
//    this->device->write({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x44,0x08,0x05,0x02,0x79,0x00,0x02,0x01,0x00,0x00,0x00,0x65,0x00 });
//    telegram = this->device->get_data(); // expect an ACK
//    this->device->write({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x82,0x0a,0x01,0x06,0x79,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x01,0xa5,0x00 });
//    telegram = this->device->get_data(); // expect an ACK
//}
*/

void PC2::event_loop(volatile bool & keepRunning) {
    while(keepRunning) {
        BOOST_LOG_TRIVIAL(info) << "Waiting on semaphore..";
        sem_wait(&this->semaphore);
        this->device->process_message(this->device->inbox.front());
        this->device->inbox.pop();
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
