#include <boost/format.hpp>
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <iostream>
#include "pc2/pc2.hpp"
#include "ml/telegram.hpp"

MasterlinkTelegram::MasterlinkTelegram(PC2Telegram & tgram) {
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
    this->telegram_type = (telegram_types)this->data[4];
    this->payload_type = (payload_types)this->data[8];
    this->payload_size = this->data[9];
    this->payload_version = this->data[10];
};


std::ostream& DisplayDataMessage::serialize(std::ostream& outputStream) {
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
};

std::ostream& MasterPresentMessage::serialize(std::ostream& outputStream) {
    std::string analysis;

    int i = 0 ;
    while(i < this->tgram.payload_size) {
        analysis.append((boost::format("%02X: %02X\n") % i % (unsigned int)this->tgram.payload[i]).str());
        i++;
    }

    return outputStream << analysis;
}

std::ostream& UnknownMessage::serialize(std::ostream& outputStream) {
    return outputStream << "Unknown message";
}

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

        Masterlink::source src;
        std::string key;
        std::string value;

        MetadataMessage(MasterlinkTelegram & tgram): DecodedMessage{tgram} {
            this->key = metadata_field_type_label[tgram.payload[0]];
            this->value = std::string(tgram.payload.begin() + 14, tgram.payload.end());
            // FIXME: Is this an appropriate constant?
            this->src = Masterlink::source::a_mem2;
        }
};

std::ostream& operator <<(std::ostream& outputStream, DecodedMessage& m) {
    m.serialize(outputStream);
    return outputStream;
};

DecodedMessage *DecodedMessageFactory::make(MasterlinkTelegram & tgram) {
    switch (tgram.payload_type) {
        case MasterlinkTelegram::payload_types::metadata:
            return new MetadataMessage(tgram);
        case MasterlinkTelegram::payload_types::display_data:
            return new DisplayDataMessage(tgram);
        case MasterlinkTelegram::payload_types::master_present:
            return new MasterPresentMessage(tgram);
        default:
            return new UnknownMessage(tgram);
    }
}
