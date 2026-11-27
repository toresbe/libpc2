#include "pc2/pc2device.hpp"
#include "pc2/pc2.hpp"
#include <boost/log/trivial.hpp>

#ifndef __PC2_MIXER_HPP
#define __PC2_MIXER_HPP

class PC2Mixer {
    PC2 *pc2;
    public:
    struct mixer_state_t {
        short unsigned int volume;          ///< Volume, range 0 to ??
        short int bass;                     ///< Bass, range -6 to 6?
        short int treble;                   ///< Treble, range -6 to 6?
        short int balance;                  ///< Balance, range -?? to ??
        bool loudness;                      ///< Loudness equalization
        bool headphones_plugged_in;         ///< Headphone plug sensor
        bool transmitting_locally;          ///< Sending to local speakers over Powerlink
        bool transmitting_from_ml;          ///< Sending to speakers from Masterlink
        bool distributing_on_ml;            ///< Distributing local audio over Masterlink
        bool speakers_muted;                ///< Speaker output muted
        bool speakers_on;                   ///< Speaker power-on signal
    } state = {};
    PC2Mixer(PC2 *pc2);
    void send_routing_state();
    void adjust_volume(int adjustment);
    /*! Absolute volume (eb 00..5a; 1 dB per step, 0 = mute). Clamped. */
    void set_volume(int volume);
    void ml_distribute(bool transmit_enabled);
    void transmit_locally(bool transmit_enabled);
    void transmit_from_ml(bool transmit_enabled);
    void speaker_mute(bool speakers_muted);
    void speaker_power(bool spakers_powered);
    void process_mixer_state(const PC2Message & tgram);
    void process_headphone_state(const PC2Message & tgram);
    /*! e3: sets the volume TARGET (applied by the fade started with ea 81), bass, treble, balance and
     *  loudness. NOTE the argument order (volume, TREBLE, BASS, ...); on the wire it is
     *  e3 [vol|loudness<<7] [bass] [treble] [balance]. Values are clamped to what the firmware accepts
     *  (volume 0..0x5a, bass/treble -8..+8 dB, balance -9..+9); it silently drops out-of-range values. */
    void set_parameters(int volume, int treble, int bass, int balance, bool loudness);
};
#endif
