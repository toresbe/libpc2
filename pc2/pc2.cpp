#include <cstdio>
#include <iomanip>

#include <iostream>
#include <string>
#include <boost/log/trivial.hpp>
#include <vector>
#include <array>

#include "pc2/pc2device.hpp"
#include "pc2/pc2.hpp"
#include "pc2/mixer.hpp"
#include "pc2/beo4.hpp"

void PC2::send_telegram(MasterlinkTelegram &tgram) {
    PC2Message msg = tgram.serialize();
    msg.insert(msg.begin(), 0xe0);
    this->device->send_telegram(msg);
}

PC2::PC2(PC2Interface * interface) {
    this->interface = interface;
    this->interface->pc2 = this;

    this->device = new PC2USBDevice;
    this->mixer = new PC2Mixer(this->device);
}

void PC2::yield(std::string description) {
    auto foo = this->device->get_data(200);
    BOOST_LOG_TRIVIAL(debug) << "(was expecting packet: " << description << ")";
    if (foo.size()) process_telegram(foo);
}

void PC2::yield() {
    auto foo = this->device->get_data(200);
    if (foo.size()) process_telegram(foo);
}

bool PC2::open() {
    this->device->open();
    return true;
}

void PC2::send_beo4_code(uint8_t dest, uint8_t code) {
    this->device->send_telegram({ 0x12, 0xe0, dest, 0xc1, 0x01, 0x0a, 0x00, 0x47, 0x00, 0x20, 0x05, 0x02, 0x00, 0x01, 0xff, 0xff, code, 0x8a, 0x00 });
};

void PC2::request_source(uint8_t source_id) {
    this->device->send_telegram({ 0x24 }); // don't know what this does, only the PC2 uses this
    this->device->get_data(); // expect 24 01
    this->device->send_telegram({0xe0, 0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 
            0x04, 0x03, 0x04, 0x0a, 0x01, 0x00, 0xe4, 0x00});
    expect_ack();
    this->device->send_telegram({ 0xe0, 0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 
            0x45, 0x07, 0x01, 0x02, source_id, 0x00, 0x02, 0x01, 
            0x00, 0x00, 0xad, 0x00, 0x61 });
    expect_ack();
    this->device->get_data(); // expect 0x44 back
    this->device->send_telegram({ 0xe4, 0x01 });
    expect_ack();
}

void PC2::process_beo4_keycode(uint8_t type, uint8_t keycode) {
    // TODO: This should offer a more semantically meaningfully separated callback structure.

    BOOST_LOG_TRIVIAL(debug) << "Got remote control code " << std::hex << std::setw(2) << std::setfill('0') << (short unsigned int) keycode;

    if(type == 0x0F && keycode == 0x0C) {
        BOOST_LOG_TRIVIAL(warning) << "We should send a Masterlink shutdown signal now!";
    }

    this->interface->beo4_press(keycode);
}

void PC2::process_telegram(PC2Telegram & tgram) {
    if (tgram[2] == 0x00) {
        process_ml_telegram(tgram);
    }
    if (tgram[2] == 0x02) {
        process_beo4_keycode(tgram[4], tgram[6]);
    }
    if (tgram[2] == 0x03) {
        // PC2 device sending mixer state
        this->mixer->process_mixer_state(tgram);
    }
    if (tgram[2] == 0x06) {
        this->mixer->process_headphone_state(tgram);
    }
}

void PC2::expect_ack() {
    PC2Telegram telegram = this->device->get_data(500);

    if (telegram[1] == 0x04) {
        if ((telegram[2] == 0x01) | (telegram[2] == 0x41)) {
            BOOST_LOG_TRIVIAL(debug) << " Got ACK.";
            if(telegram[5] != 0x01) {
                BOOST_LOG_TRIVIAL(warning) << "Masterlink not working.";
            }

        }
    } else {
        BOOST_LOG_TRIVIAL(warning) << "Expected an ACK but did not get one!";
        process_telegram(telegram);
    }
}

void PC2::send_audio() {
    PC2Telegram telegram;
    this->device->send_telegram({ 0xfa, 0x38, 0xf0, 0x88, 0x40, 0x00, 0x00 });
    telegram = this->device->get_data();
    // This is a source status (V. MASTER also sends a 0x87 telegram.) Why is it sending source 0x7A (A.MEM2)?
    BOOST_LOG_TRIVIAL(debug) << "Sending source status";
    this->device->send_telegram({ 0xe0,0x83,0xc1,0x01,0x14,0x00,0x7a,0x00,0x87,0x1e,0x04,0x7a,0x02,0x00,0x00,0x40, \
            0x28,0x01,0x00,0x00,0x00,0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, \
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x62,0x00,0x00 });
    expect_ack();
    BOOST_LOG_TRIVIAL(debug) << "Pinging Beosystem 3";
    this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x01,0x01,0xa4,0x00 });
    telegram = this->device->get_data(); // expect an ACK
    telegram = this->device->get_data(); // expect reply from V MASTER
    this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00 }); // Telegram to V. MASTER: Plz be quiet
    telegram = this->device->get_data(); // expect an ACK
    telegram = this->device->get_data(); // expect reply from V MASTER
    this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x08,0x00,0x01,0x96,0x00 });
    telegram = this->device->get_data(); // expect an ACK
    telegram = this->device->get_data(); // expect reply from V MASTER
    this->mixer->speaker_power(true);
    telegram = this->device->get_data(); // not sure what this is
    this->device->send_telegram({ 0xfa, 0x30, 0xd0, 0x65, 0x80, 0x6c, 0x22 });
    this->device->send_telegram({ 0xf9, 0x64 });
    this->mixer->set_parameters(0x22, 0, 0, 0, false);
    this->mixer->ml_distribute(true);
    this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x44,0x08,0x05,0x02,0x79,0x00,0x02,0x01,0x00,0x00,0x00,0x65,0x00 });
    telegram = this->device->get_data(); // expect an ACK
    this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x82,0x0a,0x01,0x06,0x79,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x01,0xa5,0x00 });
    telegram = this->device->get_data(); // expect an ACK
}

void PC2::event_loop(volatile bool & keepRunning) {
    while (keepRunning) {
        PC2Telegram telegram;
        telegram = this->device->get_data(300);
        if (telegram.size()) 
            process_telegram(telegram);
    }
}

void PC2::set_address_filter() {
    // I am not at all sure how this works but it seems to be a variable-length list of destination
    // address bytes to which the PC2 should respond
    switch(this->interface->address_mask) {
        case PC2Interface::address_masks::audio_master:
            BOOST_LOG_TRIVIAL(info) << "Setting address filter to audio master mode";
            this->device->send_telegram({ 0xf6, 0x10, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
            return;
        case PC2Interface::address_masks::beoport:
            BOOST_LOG_TRIVIAL(info) << "Setting address filter to Beoport PC2 mode";
            this->device->send_telegram({ 0xf6, 0x00, 0x82, 0x80, 0x83 });
            return;
        case PC2Interface::address_masks::promisc:
            BOOST_LOG_TRIVIAL(info) << "Setting address filter to promiscuous mode";
            this->device->send_telegram({ 0xf6, 0xc0, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
            return;
        default:
            BOOST_LOG_TRIVIAL(error) << "Did not get address mask!";
    }
}

void PC2::broadcast_timestamp() {
    uint8_t hour, minute, seconds;
    uint8_t day, month, year;
    std::time_t t = std::time(nullptr);
    char mbstr[100];
    if (std::strftime(mbstr, sizeof(mbstr), "%H%M%S%d%m%y", std::localtime(&t))) {
        std::sscanf(mbstr, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &hour, &minute, &seconds, &day, &month, &year);
    }
    this->device->send_telegram({ 0xe0, 0x80, 0xc1, 0x01, 0x14, 0x00, \
            0x00, 0x00, 0x40, 0x0b, 0x0b, 0x0a, \
            0x00, 0x03, hour, minute, seconds,  \
            0x00, day, month, year, 0x02, 0x76, \
            0x00 });
    this->expect_ack();
}

PC2::~PC2() {
    this->device->send_telegram({ 0xa7 });
    yield();
    yield();
}

//void PC2::send_source_status(uint8_t current_source, bool is_active);
void PC2::init() {
    this->device->reset();
    // this might be address discovery?
    this->device->send_telegram({ 0x80, 0x01, 0x00 }); // The Beomedia 1 sends 80 29 here; 80 01 seems to work, too; as does 0x00 and 0xFF
    //                                           // I don't know what 0x80 does. It generates three replies; one ACK (60 04 41 01 01 01 61), 
    //                                            // then another ACK-ish message (software/hardware version, 60 05 49 02 36 01 04 61); 
    //                                            // then another (60 02 0A 01 61, probably some escape character for a list).
    expect_ack();
    yield("Software version");
    //this->mixer->transmit_locally(true);
    //this->mixer->speaker_power(true);
    this->device->send_telegram({ 0x29 }); // Requests software version?
    // sends an ACK-ish message (60 05 49 02 36 01 04 61); then another (60 02 0A 01 61).
    set_address_filter();
    //this->device->send_telegram({0xe0, 0xC1, 0xC0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x04, 0x02, 0x04, 0x02, 0x01});
    /*this->device->send_telegram({ 0xe0, 0x83, 0xc2, 0x01, 0x14, 0x00, 0x47, 0x00, \
    //							  0x87, 0x1f, 0x04, 0x47, 0x01, 0x00, 0x00, 0x1f, \
    //							  0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x01, \
    //	                          0x00, 0x03, 0x01, 0x01, 0x01, 0x03, 0x00, 0x02, \
    //	                          0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, \
    //	                          0x00, 0x00, 0x7d, 0x00 }); */
    yield();
    yield();
    //broadcast_timestamp();
    yield();
    yield();
    //this->mixer->ml_distribute(false);

    this->device->send_telegram({ 0x26 }); // get status info (PC2 will send a 06 00/01 to say if HP are in)
    //// Check if a video master is present?
    //this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00 });
    //expect_ack();
    //yield();
    //// Some sort of status message or alive message, addressed at video master
    //this->device->send_telegram({ 0xe0,0xc0,0x01,0x01,0x0b,0x00,0x00,0x00,0x04,0x01,0x17,0x01,0xea,0x00 });
    //yield();

    //        send_beo4_code(0xc0, 0x91);
    //this->mixer->speaker_power(false);
    // look for video masters?
    //this->device->send_telegram({0xe0, 0xc0, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x0a, 0x01, 0x00, 0xe3, 0x00});
    // look for audio masters?
    //this->device->send_telegram({0xe0, 0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x0a, 0x01, 0x00, 0xe4, 0x00});
}

