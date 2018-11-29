#include <signal.h>
#include <cstdio>
#include <iomanip>

#include <iostream>
#include <string>
#include <boost/log/trivial.hpp>
#include <vector>
#include <array>

#include "amqp/amqp.hpp"
#include "pc2/pc2device.hpp"
#include "pc2/pc2.hpp"
#include "pc2/mixer.hpp"
#include "pc2/beo4.hpp"

static volatile bool keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

PC2::PC2() {
    this->source_name[ML_SRC_TV] = "TV";
    this->source_name[ML_SRC_V_MEM] = "V_MEM";
    this->source_name[ML_SRC_V_TAPE] = "V_TAPE";
    this->source_name[0x16] = "DVD_2";
    this->source_name[0x16] = "V_TAPE2";
    this->source_name[0x1F] = "SAT";
    this->source_name[0x1F] = "DTV";
    this->source_name[0x29] = "DVD";
    this->source_name[0x33] = "DTV_2";
    this->source_name[0x33] = "V_AUX";
    this->source_name[0x3E] = "V_AUX2";
    this->source_name[0x3E] = "DOORCAM";
    this->source_name[ML_SRC_PC] = "PC";
    this->source_name[0x6F] = "RADIO";
    this->source_name[0x79] = "A_MEM";
    this->source_name[0x79] = "A_TAPE";
    this->source_name[0x7A] = "A_MEM2";
    this->source_name[0x8D] = "CD";
    this->source_name[0x97] = "A_AUX";
    this->source_name[0xA1] = "N_RADIO";
    this->source_name[ML_SRC_PHONO] = "PHONO";

    this->device = new PC2USBDevice;
    this->mixer = new PC2Mixer(this->device);
    //	this->amqp = new AMQP;
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

void PC2::process_beo4_keycode(uint8_t keycode) {
    //TODO: This should be separated from "libpc2"
    BOOST_LOG_TRIVIAL(debug) << "Got remote control code " << std::hex << std::setw(2) << std::setfill('0') << (short unsigned int) keycode;

    if (Beo4::is_source_key(keycode)) {
        printf("Active source change\n");
        if (keycode == 0x91) { // A. MEM
            if (this->active_source != Beo4::source_from_keycode(keycode)) {
                //send_audio();
            } else {
                printf("Active source is %02X, new is %02X; no need to send audio\n", this->active_source, Beo4::source_from_keycode(keycode));
            }
        }

        if (keycode == BEO4_KEY_PHONO) {
            request_source(ML_SRC_PHONO);
            this->mixer->transmit_from_ml(true);
            this->mixer->speaker_power(true);
        } else if (keycode == BEO4_KEY_CD) {
            request_source(ML_SRC_CD);
            this->mixer->transmit_from_ml(true);
            this->mixer->speaker_power(true);
        } else if (keycode == BEO4_KEY_A_AUX) {
            request_source(ML_SRC_A_AUX);
            this->mixer->transmit_from_ml(true);
            this->mixer->speaker_power(true);
        } else if (keycode == BEO4_KEY_PC) {
            this->mixer->transmit_locally(true);
            this->mixer->transmit_from_ml(false);
            this->mixer->speaker_power(true);
        }

        //this->active_source = Beo4::source_from_keycode(keycode);
        if(this->amqp != nullptr) {
            // TODO: Cleaner design would be to have this in the AMQP sources!
            // this->amqp->notify_source_change or whatever
            this->amqp->set_active_source(this->active_source);
            char *hexmsg = (char *)malloc(3);
            sprintf(hexmsg, "%02X", keycode);
            this->amqp->send_message(this->source_name.at(this->active_source).c_str(), hexmsg);
            free(hexmsg);
        }
    }
    else {
        // Volume up
        if (keycode == 0x60) {
            this->mixer->adjust_volume(1);
            if(this->notify != nullptr) {
                this->notify->notify_volume(this->mixer->state.volume);
            }
        }
        // Volume down
        if (keycode == 0x64) {
            this->mixer->adjust_volume(-1);
            if(this->notify != nullptr) {
                this->notify->notify_volume(this->mixer->state.volume);
            }
        }

        if (keycode == 0x20) {
            printf("you are going insane\n");
            this->mixer->transmit_locally(true);
            this->mixer->ml_distribute(true);
            this->mixer->speaker_power(true);
        }

        // Power off
        if (keycode == 0x0c) {
            this->mixer->transmit_locally(false);
            this->mixer->speaker_power(false);
        }

        // Mute
        if (keycode == 0x0d) {
            this->mixer->speaker_mute(!this->mixer->state.speakers_muted);
        }

        // LIGHT key has been pressed; we're now in LIGHT mode for about 25 seconds
        // or until it is explicitly cancelled by the remote with 0x58.
        if (keycode == 0x9b) {
            this->last_light_timestamp = std::time(nullptr);
            printf("Entering LIGHT mode...\n");
        }
        // LIGHT mode has been explicitly cancelled
        if (keycode == 0x58) {
            this->last_light_timestamp = 0;
            printf("Leaving LIGHT mode...\n");
        }
        // If a LIGHT command has not yet timed out
        if (std::time(nullptr) <= (this->last_light_timestamp + 25)) {
            char *hexmsg = (char *)malloc(3);
            sprintf(hexmsg, "%02X", keycode);
            this->amqp->send_message("LIGHT", hexmsg);
            free(hexmsg);
        }
        else {
            if (this->active_source) {
                char *hexmsg = (char *)malloc(3);
                sprintf(hexmsg, "%02X", keycode);
                this->amqp->send_message(this->source_name.at(this->active_source).c_str(), hexmsg);
                free(hexmsg);
            }
        }
    }
}
void PC2Mixer::process_mixer_state(PC2Telegram & tgram) {
    this->state.volume = tgram[3] & 0x7f;
    this->state.loudness = tgram[3] & 0x80;
    this->state.bass = (int8_t)tgram[4];
    this->state.treble = (int8_t)tgram[5];
    this->state.balance = (int8_t)tgram[6];
    BOOST_LOG_TRIVIAL(debug) << "Got mixer state from PC2:" << \
        " vol:" << this->state.volume << \
        " bass:" << this->state.bass << \
        " trbl:" << this->state.treble << \
        " bal:" << this->state.balance << \
        " ldns: " << (this->state.loudness ? "on" : "off");
};

void PC2Mixer::process_headphone_state(PC2Telegram & tgram) {
    bool plugged_in = (tgram[3] == 0x01) ? true: false;
    this->state.headphones_plugged_in = plugged_in;
    BOOST_LOG_TRIVIAL(debug) << "Headphones are " << (plugged_in ? "" : "not ") << "plugged in";
};

void PC2::process_telegram(PC2Telegram & tgram) {
    if (tgram[2] == 0x00) {
        process_ml_telegram(tgram);
    }
    if (tgram[2] == 0x02) {
        process_beo4_keycode(tgram[6]);
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
    //this->device->send_telegram({ 0xf6, 0x10, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
    this->device->send_telegram({ 0xf6, 0x00, 0xc0 });
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
    this->device->send_telegram({ 0xf1 }); // Send initialization command.
    this->device->send_telegram({ 0x80, 0xc0, 0x00 }); // The Beomedia 1 sends 80 29 here; 80 01 seems to work, too; as does 0x00 and 0xFF
    //                                           // I don't know what 0x80 does. It generates three replies; one ACK (60 04 41 01 01 01 61), 
    //                                            // then another ACK-ish message (software/hardware version, 60 05 49 02 36 01 04 61); 
    //                                            // then another (60 02 0A 01 61, probably some escape character for a list).
    expect_ack();
    yield("Software version");
    //this->mixer->transmit_locally(true);
    //this->mixer->speaker_power(true);
    //	this->device->send_telegram({ 0x29 }); // Requests software version?
    // sends an ACK-ish message (60 05 49 02 36 01 04 61); then another (60 02 0A 01 61).
    set_address_filter();
    //this->device->send_telegram({0xe0, 0xC1, 0xC0, 0x01, 0x14, 0x00, 0x00, 0x00, 0x04, 0x02, 0x04, 0x02, 0x01});
    /*this->device->send_telegram({ 0xe0, 0x83, 0xc2, 0x01, 0x14, 0x00, 0x47, 0x00, \
    //							  0x87, 0x1f, 0x04, 0x47, 0x01, 0x00, 0x00, 0x1f, \
    //							  0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x01, \
    //	                          0x00, 0x03, 0x01, 0x01, 0x01, 0x03, 0x00, 0x02, \
    //	                          0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, \
    //	                          0x00, 0x00, 0x7d, 0x00 }); */
    //yield();
    //yield();
    //broadcast_timestamp();
    //yield();
    //yield();
    //this->mixer->ml_distribute(false);

    //this->device->send_telegram({ 0x26 }); // get status info (PC2 will send a 06 00/01 to say if HP are in)
    //// Check if a video master is present?
    //this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00 });
    //expect_ack();
    //yield();
    //// Some sort of status message or alive message, addressed at video master
    //this->device->send_telegram({ 0xe0,0xc0,0x01,0x01,0x0b,0x00,0x00,0x00,0x04,0x01,0x17,0x01,0xea,0x00 });
    //yield();
    //this->device->send_telegram({ 0xf9, 0x80 });

    //        send_beo4_code(0xc0, 0x91);
    //this->mixer->speaker_power(false);
    // look for video masters?
    this->device->send_telegram({0xe0, 0xc0, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x0a, 0x01, 0x00, 0xe3, 0x00, 0x61});
    // look for audio masters?
    this->device->send_telegram({0xe0, 0xc1, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x0a, 0x01, 0x00, 0xe4, 0x00, 0x61});
}
int main(int argc, char *argv[]) {
    if(argc == 2) {
        PC2 pc;
        pc.open();

        if(argv[1][0] == '1') {
            pc.mixer->transmit_locally(true);
            pc.mixer->speaker_power(true);
        } else if (argv[1][0] == '0') {
            pc.mixer->speaker_power(false);
        } else if (argv[1][0] == 'm') {
            pc.notify = new PC2Notifier();
            signal(SIGINT, intHandler);
            pc.init();
            pc.mixer->set_parameters(0x22, 0, 0, 0, false);
            pc.event_loop(keepRunning);
            pc.mixer->transmit_locally(false);
            pc.mixer->speaker_power(false);
        }
    } else {
        printf("Usage: %s (figure out the rest from source)\n", argv[0]);
    }
}
