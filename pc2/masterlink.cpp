#include <cstdio>
#include <ctime>
#include <string>
#include <iomanip>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <vector>
#include <array>

#include "pc2/pc2device.hpp"
#include "pc2/pc2.hpp"

void PC2::process_ml_telegram(PC2Telegram & tgram) {
	std::map<uint8_t, std::string> node_map;
	node_map[0x80] = "ALL";
	node_map[0x83] = "ALL";
	node_map[0xC0] = "V_MASTER";
	node_map[0xC1] = "A_MASTER";
	node_map[0xC2] = "PC_1";
	node_map[0x01] = "NODE_01";
	node_map[0x02] = "NODE_02";
	node_map[0x03] = "NODE_03";
	node_map[0x04] = "NODE_04";
	node_map[0x05] = "NODE_05";
	node_map[0x06] = "NODE_06";
	node_map[0x07] = "NODE_07";
	node_map[0x08] = "NODE_08";
	node_map[0x09] = "NODE_09";
	node_map[0x0A] = "NODE_0A";
	node_map[0x0B] = "NODE_0B";
	node_map[0x0C] = "NODE_0C";
	node_map[0x0D] = "NODE_0D";
	node_map[0x0E] = "NODE_0E";
	node_map[0x0F] = "NODE_0F";
	node_map[0x10] = "NODE_10";
	node_map[0x11] = "NODE_11";
	node_map[0x12] = "NODE_12";
	node_map[0x13] = "NODE_13";

	printf("| Telegram to %s from %s: \n|", node_map[tgram[3]].c_str(), node_map[tgram[4]].c_str());
	for (auto c : tgram) {
		printf(" %02X", c);
	}
	printf(".\n");
	switch (tgram[10]) {
	case(0x04):
	{
		// Some sort of audio setup
		uint8_t requesting_node = tgram[4];
		if ((tgram[11] == 0x03) && (tgram[12] == 0x04) && (tgram[14] == 0x01) && (tgram[15] == 0x00)) {
			if (tgram[13] == 0x08) {
				BOOST_LOG_TRIVIAL(info) << "Link device ping, sending pong";
			}
			else if (tgram[13] == 0x02) {
				BOOST_LOG_TRIVIAL(info) << "Video device ping? Sending pong";
			}
			this->device->send_telegram({ 0xe0, requesting_node, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x01, 0x01, 0x01, 0xe9, 0x00 });
			expect_ack();
			//this->mixer->transmit(true);
			//this->mixer->set_parameters(0x22, 0, 0, 0);
		}
	}
	break;
	case(0x08):
	{
		if ((tgram[11] == 0x00) && (tgram[12] == 0x01)) {
			BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X sent one of those 0x08 requests we don't know what are yet") % (unsigned int)tgram[4];
			// Not sure what this means but link room products will sometimes need this reply
			this->device->send_telegram({ 0xe0, tgram[4], 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0xe4, 0x00 });
			//							A casting AM will yield this reply in stead
            //         			         '0xe0, 0xc0,     0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x08, 0x04, 0x06, 0x02, 0x01, 0x00, 0x79, 0x24, 0x00'

			expect_ack();
		}
		else {
			BOOST_LOG_TRIVIAL(warning) << boost::format("Node #%02X sent us a 0x08 request we don't know how to handle!") % (unsigned int)tgram[4];
		}
	}
	break;
        case(0x82):
        {
            std::string source_name;
            if(this->source_name.count(tgram[14])) {
                source_name = this->source_name[tgram[14]];
            } else {
                source_name = "unknown source";
            }
            
            std::string state;
            if (tgram[16] == ML_STATE_STOPPED) state = "stopped";
            else if (tgram[16] == ML_STATE_PLAYING) state = "playing";
            else if (tgram[16] == ML_STATE_PLAYING) state = "fast-forwarding";
            else if (tgram[16] == ML_STATE_PLAYING) state = "rewinding";
            else state = "unknown";
        //14 = source, 15 = track, 16 = playstate
            
            BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X is %s source %s (track %d)") % (unsigned int)tgram[4] % state % source_name % (unsigned int) tgram[15];

//        60 14 00 01 C1 01 14 00 00 00 82 09 01 04 8D 06 02 00 00 01 FF FF 61
        }
        break;
	case(0x10):
		if (!memcmp("\x10\x03\x03\x01\x00\x01", (void *)(tgram.data() + 10), 6)) {
			BOOST_LOG_TRIVIAL(info) << "Video master is requesting audio control back. Relinquishing...";
			this->mixer->ml_distribute(false);
		}
		break;
	case(0x11):
	{
		if ((tgram[11] == 0x02) && (tgram[12] == 0x02) && (tgram[13] == 0x01) && (tgram[14] == 0x00)) {
			BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X powered down") % (unsigned int)tgram[4];
		}
		else {
			BOOST_LOG_TRIVIAL(warning) << boost::format("Node #%02X sent us a 0x11 telegram we don't know how to handle!") % (unsigned int)tgram[4];
		}
	}
	break;
	case(0x30):
	{
		if ((tgram[11] == 0x00) && (tgram[12] == 0x02)) {
			BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X sent one of those 0x30 requests we don't know what are yet") % (unsigned int)tgram[4];
			// Not sure what this means but link room products will sometimes need this reply
			this->device->send_telegram({ 0xe0, tgram[4], 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x30, 0x00, 0x04, 0x0c, 0x00 });
			expect_ack();
		}
		else {
			BOOST_LOG_TRIVIAL(warning) << boost::format("Node #%02X sent us a 0x30 request we don't know how to handle!") % (unsigned int)tgram[4];
		}
	}
	break;
	case(0x40):
		printf("| Time is: %02X:%02X:%02X on 20%02X-%02X-%02X.\n", tgram[16], tgram[17], tgram[18], tgram[22], tgram[21], tgram[20]);
		break;
	case(0x44):
		// So, I don't know what this telegram type actually means yet or how to decode it, however I have observed it in 
		// contexts where audio bus ownership is handed from one device to another. Erring on the side of caution I've decided 
		// to just cut the feed if this should ever occur (it's not in any "supported" configuration for this code).
		//BOOST_LOG_TRIVIAL(warning) << "Disabling audio output because we got a 0x44 telegram and I don't know what that means (see code)";
		//this->mixer->ml_distribute(false);
		break;
	case(0x45):
	{
		uint8_t source_num = tgram[14];
		uint8_t requesting_node = tgram[4];
		BOOST_LOG_TRIVIAL(info) << boost::format("Source %02X requested by node #%02X") % (unsigned int)source_num % (unsigned int)requesting_node;
		this->listener_count++;
		this->mixer->ml_distribute(true);
		this->mixer->set_parameters(0x22, 0, 0, 0, false);
		this->device->send_telegram({ 0xe0, 0x83, 0xc1, 0x01, 0x14, 0x00, source_num, 0x00, 0x87, 0x1f, 0x04, source_num, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x02, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe2, 0x00 });
		expect_ack();
		this->device->send_telegram({ 0xe0, requesting_node, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x44, 0x08, 0x05, 0x02, source_num, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0xaa, 0x00 });
		expect_ack();
		this->device->send_telegram({ 0xe0, requesting_node, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x82, 0x0a, 0x01, 0x06, source_num, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xea, 0x00 });
		expect_ack();
	}
	break;
	case(0x87):
		printf("| Active source is: 0x%02X.\n", tgram[13]);
		break;
	}


};
