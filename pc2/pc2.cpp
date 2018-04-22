#include <cstdio>

#include <string>
#include <boost/log/trivial.hpp>
#include <vector>
#include <array>

#include "amqp/amqp.hpp"
#include "pc2/pc2device.hpp"
#include "pc2/pc2.hpp"

PC2Mixer::PC2Mixer(PC2USBDevice *device) {
	this->device = device;
}

void PC2Mixer::transmit(bool transmit_enabled) {
	if (transmit_enabled == true) {
		this->device->send_telegram({ 0xe7, 0x00 });
		this->device->send_telegram({ 0xe5, 0x00, 0x01, 0x00, 0x00 });
	}
	else {
		this->device->send_telegram({ 0xe5, 0x00, 0x00, 0x00, 0x01 });
		this->device->send_telegram({ 0xe7, 0x01 });
	}
};

void PC2Mixer::set_parameters(uint8_t volume, uint8_t treble, uint8_t bass, uint8_t balance) {
	this->device->send_telegram({ 0xe3, volume, treble, bass, balance });
}


class Beo4 {
public:
	static uint8_t source_from_keycode(uint8_t beo4_code) {
		std::map<uint8_t, uint8_t> map;
		map[0x80] = 0x0B;
		map[0x81] = 0x6F;
		map[0x82] = 0x33;
		map[0x85] = 0x16;
		map[0x86] = 0x29;
		map[0x8A] = 0x1F;
		map[0x8B] = 0x47;
		map[0x8D] = 0x3E;
		map[0x91] = 0x79;
		map[0x92] = 0x8D;
		map[0x93] = 0xA1;
		return map[beo4_code];
	}

	static bool is_source_key(uint8_t beo4_code) {
		std::vector<uint8_t> source_keys = { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,\
											  0x88, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x90, 0x91,\
											  0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0xA8, 0x47,\
											  0x0C, 0xFA };
		for (auto i : source_keys) {
			if (beo4_code == i) return true;
		}

		return false;
	}
};

PC2::PC2() {
	this->source_name[0x0B] = "TV";
	this->source_name[0x15] = "V_MEM";
	this->source_name[0x15] = "V_TAPE";
	this->source_name[0x16] = "DVD_2";
	this->source_name[0x16] = "V_TAPE2";
	this->source_name[0x1F] = "SAT";
	this->source_name[0x1F] = "DTV";
	this->source_name[0x29] = "DVD";
	this->source_name[0x33] = "DTV_2";
	this->source_name[0x33] = "V_AUX";
	this->source_name[0x3E] = "V_AUX2";
	this->source_name[0x3E] = "DOORCAM";
	this->source_name[0x47] = "PC";
	this->source_name[0x6F] = "RADIO";
	this->source_name[0x79] = "A_MEM";
	this->source_name[0x7A] = "A_MEM2";
	this->source_name[0x8D] = "CD";
	this->source_name[0x97] = "A_AUX";
	this->source_name[0xA1] = "N_RADIO";

	this->device = new PC2USBDevice;
	this->mixer = new PC2Mixer(this->device);
	this->amqp = new AMQP;
}

void PC2::yield() {
	auto foo = this->device->get_data(200);
	if (foo.size()) process_telegram(foo);
}

bool PC2::open() {
	this->device->open();
	init();
	return true;
}



void PC2::send_beo4_code(uint8_t dest, uint8_t code) {
	this->device->send_telegram({ 0x12, 0xe0, dest, 0xc1, 0x01, 0x0a, 0x00, 0x47, 0x00, 0x20, 0x05, 0x02, 0x00, 0x01, 0xff, 0xff, code, 0x8a, 0x00 });
};

void PC2::process_beo4_keycode(uint8_t keycode) {
	printf("Got remote control code %02x\n", keycode);
	if (Beo4::is_source_key(keycode)) {
		printf("Active source change\n");
		if (keycode == 0x91) { // A. MEM
			if (this->active_source != Beo4::source_from_keycode(keycode)) {
				//send_audio();
			} else {
				printf("Active source is %02X, new is %02X; no need to send audio\n", this->active_source, Beo4::source_from_keycode(keycode));
			}
		}
		this->active_source = Beo4::source_from_keycode(keycode);
		this->amqp->set_active_source(this->active_source);
	}
	else {
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

void PC2::process_telegram(PC2Telegram & tgram) {
	if (tgram[2] == 0x00) {
		process_ml_telegram(tgram);
	}
	if (tgram[2] == 0x02) {
		process_beo4_keycode(tgram[6]);
	}
}

void PC2::expect_ack() {
	PC2Telegram telegram = this->device->get_data(500);
	if (memcmp("\x60\x04\x41\x01\x01\x01\x61", (void *)telegram.data(), 7)) {
		if (memcmp("\x60\x04\x01\x01\x01\x01\x61", (void *)telegram.data(), 7)) {
			printf("Expected an ACK but did not find one!\n");
			process_telegram(telegram);
		}
	}
}

void PC2::send_audio() {
	PC2Telegram telegram;
	this->device->send_telegram({ 0xfa, 0x38, 0xf0, 0x88, 0x40, 0x00, 0x00 });
	telegram = this->device->get_data();
	// This is a source status (V. MASTER also sends a 0x87 telegram.) Why is it sending source 0x7A (A.MEM2)?
	this->device->send_telegram({ 0xe0,0x83,0xc1,0x01,0x14,0x00,0x7a,0x00,0x87,0x1e,0x04,0x7a,0x02,0x00,0x00,0x40, \
			0x28,0x01,0x00,0x00,0x00,0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, \
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x62,0x00,0x00 });
	telegram = this->device->get_data(); // expect an ACK
	this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x01,0x01,0xa4,0x00 });
	telegram = this->device->get_data(); // expect an ACK
	telegram = this->device->get_data(); // expect reply from V MASTER
	this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00 }); // Telegram to V. MASTER: Plz be quiet
	telegram = this->device->get_data(); // expect an ACK
	telegram = this->device->get_data(); // expect reply from V MASTER
	this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x08,0x00,0x01,0x96,0x00 });
	telegram = this->device->get_data(); // expect an ACK
	telegram = this->device->get_data(); // expect reply from V MASTER
	this->device->send_telegram({ 0xea, 0xff }); // does not generate a reply
	this->device->send_telegram({ 0xea, 0x81 }); // this, however, does
	telegram = this->device->get_data(); // not sure what this is
	this->device->send_telegram({ 0xfa, 0x30, 0xd0, 0x65, 0x80, 0x6c, 0x22 });
	this->device->send_telegram({ 0xf9, 0x64 });
	this->mixer->set_parameters(0x22, 0, 0, 0);
	this->mixer->transmit(true);
	this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x44,0x08,0x05,0x02,0x79,0x00,0x02,0x01,0x00,0x00,0x00,0x65,0x00 });
	telegram = this->device->get_data(); // expect an ACK
	this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x82,0x0a,0x01,0x06,0x79,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x01,0xa5,0x00 });
	telegram = this->device->get_data(); // expect an ACK
}

void PC2::event_loop() {
	while (1) {
		PC2Telegram telegram;

		telegram = this->device->get_data();
		process_telegram(telegram);
	}
}
void PC2::set_address_filter() {
	// I am not at all sure how this works but it seems to be a variable-length list of destination
	// address bytes to which the PC2 should respond
	this->device->send_telegram({ 0xf6, 0x10, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
}

void PC2::broadcast_timestamp() {
	uint8_t hour, minute, seconds;
	uint8_t day, month, year;
	std::time_t t = std::time(nullptr);
	char mbstr[100];
	if (std::strftime(mbstr, sizeof(mbstr), "%H%M%S%d%m%y", std::localtime(&t))) {
		std::sscanf(mbstr, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &hour, &minute, &seconds, &day, &month, &year);
	}
	this->device->send_telegram({ 0xe0, 0x80, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x40, 0x0b, 0x0b, 0x0a, 0x00, 0x03, hour, minute, seconds, 0x00, day, month, year, 0x02, 0x76, 0x00 });
	this->expect_ack();
}

PC2::~PC2() {
	this->device->send_telegram({ 0xa7 });
	yield();
	yield();
}

void PC2::init() {
	yield();
	yield();
	yield();
	this->device->send_telegram({ 0xf1 });
	this->device->send_telegram({ 0x80, 0x01, 0x00 }); // The Beomedia 1 sends 80 29 here; 80 01 seems to work, too; as does 0x00 and 0xFF
													   // I don't know what 0x80 does. It generates three replies; one ACK (60 04 41 01 01 01 61), 
	expect_ack();
	// then another ACK-ish message (60 05 49 02 36 01 04 61); then another (60 02 0A 01 61).
	yield();
	yield();
	this->device->send_telegram({ 0x29 }); // Don't know what this does.
										   // sends an ACK-ish message (60 05 49 02 36 01 04 61); then another (60 02 0A 01 61).
	yield();
	yield();
	set_address_filter();
	this->device->send_telegram({ 0xe0, 0x83, 0xc2, 0x01, 0x14, 0x00, 0x47, 0x00, \
								  0x87, 0x1f, 0x04, 0x47, 0x01, 0x00, 0x00, 0x1f, \
								  0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x01, \
		                          0x00, 0x03, 0x01, 0x01, 0x01, 0x03, 0x00, 0x02, \
		                          0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, \
		                          0x00, 0x00, 0x7d, 0x00 });
	yield();
	yield();
	broadcast_timestamp();
	yield();
	yield();
	this->mixer->transmit(false);

	yield();
	this->device->send_telegram({ 0x26 });
	this->device->send_telegram({ 0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00 }); 
	expect_ack();
	this->device->send_telegram({ 0xe0,0xc0,0x01,0x01,0x0b,0x00,0x00,0x00,0x04,0x01,0x17,0x01,0xea,0x00 });
	expect_ack();
	yield();
	this->device->send_telegram({ 0xf9, 0x80 });

	//        send_beo4_code(0xc0, 0x91);
}
int main() {
	PC2 pc;
	pc.open();
	pc.event_loop();
}
