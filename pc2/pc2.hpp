#ifndef __PC2_HPP
#define __PC2_HPP

#include <vector>
#include <cstdint>
#include <ctime>
#include <map>

#include "pc2/pc2device.hpp"
#include "amqp/amqp.hpp"

class PC2Mixer {
	PC2USBDevice *device;
        struct state {
            short unsigned int volume;
            short int bass;
            short int treble;
            short int balance;
            bool loudness;
            bool headphones_plugged_in;
            bool transmitting_locally;
            bool distributing_on_ml;
            bool speakers_muted;
            bool speakers_on;
        } state;
        void send_routing_state();
public:
	PC2Mixer(PC2USBDevice *device);
        void adjust_volume(int adjustment);
	void ml_distribute(bool transmit_enabled);
	void transmit_locally(bool transmit_enabled);
        void speaker_mute(bool speakers_muted);
        void speaker_power(bool spakers_powered);
        void process_mixer_state(PC2Telegram & tgram);
        void process_headphone_state(PC2Telegram & tgram);
	void set_parameters(uint8_t volume, uint8_t treble, uint8_t bass, uint8_t balance, bool loudness);
};

class PC2 {
	PC2USBDevice *device;
	PC2Mixer *mixer;
	AMQP *amqp;
	uint8_t active_source = 0;
	unsigned int listener_count = 0;
	std::map<uint8_t, std::string> source_name;
	std::time_t last_light_timestamp = 0;

public:
	PC2();
	void yield();
	void yield(std::string description);
	void set_address_filter();
	void broadcast_timestamp();
	~PC2();
	void init();
	bool open();
	void process_ml_telegram(PC2Telegram & tgram);
	void send_beo4_code(uint8_t dest, uint8_t code);
	void process_beo4_keycode(uint8_t keycode);
	void process_telegram(PC2Telegram & tgram);
	void expect_ack();
	void send_audio();
	void event_loop();
};

#endif
