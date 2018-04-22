#include <string>
#include "ml/masterlink.hpp"

MasterlinkSource::MasterlinkSource(PC2 *pc2) {
	this->pc2 = pc2;
}

class AudioAuxSource : MasterlinkSource {
	uint8_t id = 0x79;
	std::string amqp_name = "A_MEM";
	std::string short_name = "A. MEM";
	bool audio_handled_here = true;
};
