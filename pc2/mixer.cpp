#include "pc2/mixer.hpp"

PC2Mixer::PC2Mixer(PC2USBDevice *device) {
    this->device = device;
}

void PC2Mixer::speaker_mute(bool is_muted) {
    uint8_t mute_command = is_muted ? 0x80 : 0x81;
    this->device->send_telegram({ 0xea, mute_command });
    this->state.speakers_muted = is_muted;
}

void PC2Mixer::speaker_power(bool is_powered) {
    // B&O software will always unmute after turning power on,
    // and mute before turning off. I don't know if there's any
    // reason for this but I have observed the PC2 crashing 
    // very hard if this is fudged, so - I'm going to do what 
    // B&O does.
    uint8_t power_command = is_powered ? 0xFF : 0x00;

    if (is_powered) {
        this->device->send_telegram({ 0xea, power_command });
        this->speaker_mute(false);
    } else {
        this->speaker_mute(true);
        this->device->send_telegram({ 0xea, power_command });
    }

    this->state.speakers_on = is_powered;
}

void PC2Mixer::adjust_volume(int adjustment) {
    // Gradually adjusting volume avoids a square pulse.
    // I've always gotten those pulses on both my Beosystem 7000 and
    // this PC2 too, but that might just be bad caps I guess?
    if (!adjustment) return;

    int increment = (adjustment > 0) ? 1 : -1;

    for(int i = 0; adjustment != i; i += increment) {
        BOOST_LOG_TRIVIAL(info) << "adjusting volume";
        this->device->send_telegram({ 0xeb, \
                (adjustment > 0) ? (uint8_t)0x80 : (uint8_t)0x81});
    }
}

void PC2Mixer::send_routing_state() {
    // For an as-yet-unknown reason, when the device is neither transmitting
    // or distributing, software always seems to assert the fourth byte.
    // I don't know what it does and have not examined closely what 
    // effect it has.
    uint8_t muted = 0x00;

    if(! (this->state.distributing_on_ml || this->state.transmitting_locally)) 
        muted = 0x01;

    uint8_t distribute = this->state.distributing_on_ml ? 0x01 : 0x00;

    uint8_t locally;

    if (this->state.transmitting_locally && this->state.transmitting_from_ml)
        locally = 0x03;
    else if (this->state.transmitting_from_ml)
        locally = 0x04;
    else if (this->state.transmitting_locally)
        locally = 0x01;
    else
        locally = 0x00;


    this->device->send_telegram({ 0xe7, muted });
    this->device->send_telegram({ 0xe5, locally, distribute, 0x00, muted });
}

void PC2Mixer::transmit_from_ml(bool transmit_enabled) {
    if (this->state.transmitting_from_ml != transmit_enabled) {
        BOOST_LOG_TRIVIAL(info) << "Setting local output";
        this->state.transmitting_from_ml = transmit_enabled;
        this->send_routing_state();
    } else {
        BOOST_LOG_TRIVIAL(info) << "Local ML output already in requested state";
    }
};

void PC2Mixer::transmit_locally(bool transmit_enabled) {
    if (this->state.transmitting_locally != transmit_enabled) {
        BOOST_LOG_TRIVIAL(info) << "Setting local output";
        this->state.transmitting_locally = transmit_enabled;
        this->send_routing_state();
    } else {
        BOOST_LOG_TRIVIAL(info) << "Local output already in requested state";
    }
};

void PC2Mixer::ml_distribute(bool transmit_enabled) {
    if (this->state.distributing_on_ml != transmit_enabled) {
        BOOST_LOG_TRIVIAL(info) << "Setting ML distribution";
        this->state.distributing_on_ml = transmit_enabled;
        this->send_routing_state();
    } else {
        BOOST_LOG_TRIVIAL(info) << "ML distribution already in requested state";
    }
};

void PC2Mixer::set_parameters(uint8_t volume, uint8_t treble, uint8_t bass, uint8_t balance, bool loudness) {
    // Loudness equalization is the most significant byte of the 
    // volume control byte.
    uint8_t vol_byte = volume | (loudness ? 0x80 : 0x00);

    this->device->send_telegram({ 0xe3, vol_byte, bass, treble, balance });
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
