#include "pc2.hpp"

class MasterlinkSource {
public:
	uint8_t id;
	std::string amqp_name;
	std::string short_name;
	bool audio_handled_here; // whether or not this source should trigger a masterlink audio change
	PC2 *pc2;

	//virtual void activate() = 0;
	//
	MasterlinkSource(PC2 *pc2);
};