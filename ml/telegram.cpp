#include <boost/format.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include "pc2/pc2.hpp"

#ifndef __TELEGRAM_CPP
#define __TELEGRAM_CPP

typedef std::vector<std::string> payload_labels_t;
typedef std::vector<std::pair<bool, uint8_t>> payload_expectations_t;
namespace Beolink {
    enum source {
        n_radio = 0xa1,
        a_mem_2 = 0x7a,
    };
};

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

        enum payload_type {
            master_present = 0x04,
            display_data = 0x06,
            metadata = 0x0b,
        };

        std::map<uint8_t, std::string> payload_type_name = {
            {payload_type::master_present, "MASTER_PRESENT"},
            {0x05, "???"},
            {payload_type::display_data, "DISPLAY_DATA"},
            {0x08, "???"},
            {payload_type::metadata, "METADATA"},
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
            {0x40, "TIME"}, // not seen so far, but a forum post suggests this exists...
            {0x5e, "CONFIG"},
        };
        ;

        std::vector<uint8_t> data;
        std::vector<uint8_t> payload;
        uint8_t dest_node;
        uint8_t src_node;
        uint8_t payload_type;
        unsigned int payload_size;
        unsigned int payload_version;
        uint8_t telegram_type;

        MasterlinkTelegram(PC2Telegram & tgram) {
            assert(tgram[0] == 0x60); // A PC2 telegram always starts with 0x60
            //assert(tgram[2] == 0x00); // A Masterlink telegram always starts with 0x00

            std::vector<uint8_t>::const_iterator first = tgram.begin() + 2;
            std::vector<uint8_t>::const_iterator last = tgram.end() - 1;
            this->data = std::vector<uint8_t>(first, last);
            // strip off checksum and EOT
            first = tgram.begin() + 13;
            last = tgram.end() - 1;
            this->payload = std::vector<uint8_t>(first, last);

            this->dest_node = this->data[1];
            this->src_node = this->data[2];
            this->telegram_type = this->data[4];
            this->payload_type = this->data[8];
            this->payload_size = this->data[9];
            this->payload_version = this->data[10];
        }
};

typedef uint8_t ml_src_id;
class BeolinkSource {
    public:
        BeolinkSource(ml_src_id src){
        }
};

class DecodedMessage {
    public:
        virtual std::ostream& serialize(std::ostream& outputStream) = 0;
        friend std::ostream& operator <<(std::ostream& outputStream, const DecodedMessage& m);
        MasterlinkTelegram tgram;
        DecodedMessage(MasterlinkTelegram & tgram): tgram{tgram} { };
};

class DisplayDataMessage: public DecodedMessage {
    public:
        DisplayDataMessage(MasterlinkTelegram & tgram): DecodedMessage{tgram} { }
        std::ostream& serialize(std::ostream& outputStream) {
            std::string analysis;

            int i = 0;
            bool surprises = false;

            if(this->tgram.payload_size != 17) {
                analysis.append("Unexpected length of display data!\n");
            }

            std::vector<std::pair<bool, uint8_t>> expectations = {
                {true, 0x03},
                {false, 0x01}, // this is sometimes 0x02
                {true, 0x01},
                {true, 0x00},
                {true, 0x00}
            };

            while(i < expectations.size()) {
                if(this->tgram.payload[i] != expectations[i].second) {
                    if(expectations[i].first) {
                        surprises = true;
                        analysis.append("Found unexpected data!\n");
                    }
                }
                i++;
            }
            if(surprises) {
                i = 0;
                while(i < expectations.size()) {
                    analysis.append((boost::format("%02X: %02X (expected %02X)\n") % i \
                                % (unsigned int)this->tgram.payload[i] \
                                % (unsigned int)expectations[i].second).str());
                    i++;
                }
            }

            analysis.append("Display data: [");
            analysis.append(std::string(this->tgram.payload.begin() + 5, this->tgram.payload.end() - 2));
            analysis.append("]");
            return outputStream << analysis;
        }
};
class MasterPresentMessage: public DecodedMessage {
    public:
        MasterPresentMessage(MasterlinkTelegram & tgram): DecodedMessage{tgram} { }

        std::ostream& serialize(std::ostream& outputStream) {
            std::string analysis;

            int i = 0 ;
            while(i < this->tgram.payload_size) {
                analysis.append((boost::format("%02X: %02X\n") % i % (unsigned int)this->tgram.payload[i]).str());
                i++;
            }

            return outputStream << analysis;
        }
};

class UnknownMessage: public DecodedMessage {
    public:
        std::ostream& serialize(std::ostream& outputStream) {
            return outputStream << "Unknown message";
        }
        UnknownMessage(MasterlinkTelegram & tgram): DecodedMessage{tgram} { }
};


class MetadataMessage: public DecodedMessage {
    public:
        std::ostream& serialize(std::ostream& outputStream) {
            return outputStream << "Metadata message: " << this->key << ": \"" << this->value << "\"";
        }

        payload_labels_t labels = {"Field type ID", "??", "??", "??", "Metadata type: N_RADIO or A_MEM2", "??", "??", "??", "??", "??", "??", "??", "??", "??"};
        payload_expectations_t expectations = {
            {false, 0},
            {true, 0},
            {true, 1},
            {true, 1},
            {false, 0}, // Metadata message type? If radio, gives 1A (ML SRC N_RADIO), if A MEM gives 7A (A_MEM2)
            {true, 5},
            {true, 0},
            {true, 0},
            {true, 0},
            {true, 0xFF},
            {true, 0},
            {true, 0xFF},
            {true, 0},
            {true, 1},
        };

        bool any_surprises_here() {
            unsigned int i = 0;
            std::string analysis = "";
            bool anything_unexpected = false;
            while(i < 14) {
                // Do we expect a specific value?
                if(this->expectations[i].first) {
                    // Is it the value we're expecting?
                    if(this->tgram.payload[i] != this->expectations[i].second) {
                        anything_unexpected = true;
                        analysis.append("\x1b[91m");
                        analysis.append(boost::str(boost::format("Expected %02X") % this->expectations[i].second));
                    } else {
                        analysis.append("\x1b[92m");
                    }
                } else {
                        analysis.append("\x1b[93m");
                }

                analysis.append(boost::str(boost::format("\t%02d: %02X [%s]\x1b[0m\n") % i % this->tgram.payload[i] % labels[i]));
                i++;
            }
            if (anything_unexpected)
                std::cout << analysis;
        };

        friend std::ostream& operator <<(std::ostream& outputStream, const MetadataMessage& m);
        enum metadata_message_type {
            amem = 0x7a,
            radio = 0xa1
        };

        enum metadata_field_type {
            genre = 0x01,
            album = 0x02,
            artist = 0x03,
            track = 0x04,
        };

        std::map<uint8_t, std::string> metadata_field_type_label {
            {0x01, "genre"},
                {0x02, "album"},
                {0x03, "artist"},
                {0x04, "track"},
        };

        BeolinkSource *src;
        std::string key;
        std::string value;

        MetadataMessage(MasterlinkTelegram & tgram): DecodedMessage{tgram} {
            this->key = metadata_field_type_label[tgram.payload[0]];
            this->value = std::string(tgram.payload.begin() + 14, tgram.payload.end());
            this->src = new BeolinkSource((ml_src_id) 0x7a);
        }
};

std::ostream& operator <<(std::ostream& outputStream, DecodedMessage& m) {
    m.serialize(outputStream);
    return outputStream;
}

class DecodedMessageFactory {
    public:
        static DecodedMessage *make(MasterlinkTelegram & tgram) {
            switch (tgram.payload_type) {
                case MasterlinkTelegram::payload_type::metadata:
                    return new MetadataMessage(tgram);
                case MasterlinkTelegram::payload_type::display_data:
                    return new DisplayDataMessage(tgram);
                case MasterlinkTelegram::payload_type::master_present:
                    return new MasterPresentMessage(tgram);
                default:
                    return new UnknownMessage(tgram);
            }
        }
};
#endif
