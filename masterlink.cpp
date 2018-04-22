#include <string>
#include "masterlink.hpp"

class MasterlinkSource {
public:
	uint8_t id;
	std::string amqp_name;
	std::string short_name;
	bool audio_handled_here; // whether or not this source should trigger a masterlink audio change
	PC2 *pc2;

	//virtual void activate() = 0;
	//
	MasterlinkSource(PC2 *pc2) {
		this->pc2 = pc2;
	}
};

class AMQPForwardingSource {
	void beo4_keypress(uint8_t keycode) {

	}
};

class AudioAuxSource : MasterlinkSource {
	uint8_t id = 0x79;
	std::string amqp_name = "A_MEM";
	std::string short_name = "A. MEM";
	bool audio_handled_here = true;
};
