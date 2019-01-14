#include <vector>
#include <cstdint>
#include "pc2/pc2.hpp"

#ifndef __TELEGRAM_CPP
#define __TELEGRAM_CPP 

class MasterlinkTelegram {
    public:
	std::map<uint8_t, std::string> node_name = {
            {0x80, "ALL"},
            {0x83, "ALL"},
            {0xC0, "V_MASTER"},
            {0xC1, "A_MASTER"},
            {0xC2, "PC_1"},
            {0x01, "NODE_01"},
            {0x02, "NODE_02"},
            {0x03, "NODE_03"},
            {0x04, "NODE_04"},
            {0x05, "NODE_05"},
            {0x06, "NODE_06"},
            {0x07, "NODE_07"},
            {0x08, "NODE_08"},
            {0x09, "NODE_09"},
            {0x0A, "NODE_0A"},
            {0x0B, "NODE_0B"},
            {0x0C, "NODE_0C"},
            {0x0D, "NODE_0D"},
            {0x0E, "NODE_0E"},
            {0x0F, "NODE_0F"},
            {0x10, "NODE_10"},
            {0x11, "NODE_11"},
            {0x12, "NODE_12"},
            {0x13, "NODE_13"},
        };

	std::map<uint8_t, std::string> payload_type_name = {
            {0x04, "MASTER_PRESENT"},
            {0x05, "???"},
            {0x06, "???"},
            {0x08, "???"},
            {0x0b, "???"},
            {0x0d, "BEO4_KEY"},
            {0x10, "STANDBY"},
            {0x11, "RELEASE"},
            {0x12, "???"},
            {0x20, "???"},
            {0x30, "???"},
            {0x3c, "TIMER"},
            {0x40, "CLOCK"},
            {0x44, "TRACK_INFO"},
            {0x45, "GOTO_SOURCE"},
            {0x5c, "???"},
            {0x6c, "DISTRIBUTION_REQUEST"},
            {0x82, "TRACK_INFO_LONG"},
            {0x87, "STATUS_INFO"},
            {0x94, "DVD_STATUS_INFO"},
            {0x96, "PC_PRESENT"},
        };

	std::map<uint8_t, std::string> telegram_type_name = {
	    {0x0a, "COMMAND"},
	    {0x0b, "REQUEST"},
	    {0x14, "STATUS"},
	    {0x2c, "INFO"},
	    {0x40, "TIME"},
	    {0x5e, "CONFIG"},
	};
// not seen so far, but a forum post suggests this exists... 
;

        std::vector<uint8_t> data;
        uint8_t dest_node;
        uint8_t src_node;
        uint8_t payload_type;
        unsigned int payload_size;
        uint8_t telegram_type;
        
        MasterlinkTelegram(PC2Telegram & tgram) {
            assert(tgram[0] == 0x60); // A PC2 telegram always starts with 0x60
            assert(tgram[2] == 0x00); // A Masterlink telegram always starts with 0x00

            std::vector<uint8_t>::const_iterator first = tgram.begin() + 2;
            std::vector<uint8_t>::const_iterator last = tgram.end() - 1;
            this->data = std::vector<uint8_t>(first, last);

            this->dest_node = this->data[1];
            this->src_node = this->data[2];
            this->telegram_type = this->data[4];
            this->payload_type = this->data[8];
            this->payload_size = this->data[9];
        }
};

#endif
