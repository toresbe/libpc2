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
public:
	PC2Mixer(PC2USBDevice *device);
	void transmit(bool transmit_enabled);
	void set_parameters(uint8_t volume, uint8_t treble, uint8_t bass, uint8_t balance);
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