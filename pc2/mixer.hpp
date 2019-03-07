#include "pc2/pc2device.hpp"
#include "pc2/pc2.hpp"
#include <boost/log/trivial.hpp>

#ifndef __PC2_MIXER_HPP
#define __PC2_MIXER_HPP

class PC2Mixer {
    PC2 *pc2;
    public:
    struct state {
        short unsigned int volume;
        short int bass;
        short int treble;
        short int balance;
        bool loudness;
        bool headphones_plugged_in;
        bool transmitting_locally;
        bool transmitting_from_ml;
        bool distributing_on_ml;
        bool speakers_muted;
        bool speakers_on;
    } state;
    PC2Mixer(PC2 *pc2);
    void send_routing_state();
    void adjust_volume(int adjustment);
    void ml_distribute(bool transmit_enabled);
    void transmit_locally(bool transmit_enabled);
    void transmit_from_ml(bool transmit_enabled);
    void speaker_mute(bool speakers_muted);
    void speaker_power(bool spakers_powered);
    void process_mixer_state(PC2Telegram & tgram);
    void process_headphone_state(PC2Telegram & tgram);
    void set_parameters(uint8_t volume, uint8_t treble, uint8_t bass, uint8_t balance, bool loudness);
};
#endif
