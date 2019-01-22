#include "ml/pprint.cpp"
#include "ml/telegram.cpp"
#include <cstdio>
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include "pc2/pc2.hpp"

#define BLKTP_SECTION_HEADER 0x0A0D0D0A
#define BLKTP_ENHANCED_PACKET 0x00000006

class PcapngReader {
    FILE *packetfile;

    void dump_comment(uint8_t *extra_data_block) {
        if ((((uint16_t *)extra_data_block)[0]) == 1) {
            printf("\x1b[30;106mComment:\n");
            unsigned int extra_data_block_len = ((uint16_t *)extra_data_block)[1];
            int i = 0;
            while (i < extra_data_block_len) {
                putchar(extra_data_block[4 + i++]);
            }
            printf("\x1b[0m\n");
        }
    }

    PC2Telegram read_enhanced_packet(void * block_data, unsigned int block_size) {
        unsigned int packet_len = ((uint32_t *)block_data)[3];
        unsigned int packet_len_padded = ((packet_len + (4 - 1)) & -4);
        uint32_t *block_data_32 = (uint32_t *)block_data;
        uint8_t *packet_data = (uint8_t *)(block_data_32 + 5);
        unsigned int usb_pseudoheader_len = ((uint16_t *)packet_data)[0];
        uint8_t direction = packet_data[16];
        int i = usb_pseudoheader_len;

        // Is there a comment?
        if (packet_len_padded < block_size - 32) {
            dump_comment(packet_data+packet_len_padded);
        }

        return PC2Telegram(packet_data + usb_pseudoheader_len, packet_data + packet_len); 
    }

    public:
    bool eof = false;

    PcapngReader(std::string filespec) {
        packetfile = fopen(filespec.c_str(), "r");
    }

    std::vector<uint8_t> get_next_packet() {
        uint32_t block_type;
        uint32_t block_size;
        uint32_t block_size_spare;

        void *block_data;
        uint8_t *block_data_8;
        uint32_t *block_data_32;

        if (fread(&block_type, sizeof(uint32_t), 1, packetfile) == EOF) {
            this->eof = true;
            return {};
        }

        fread(&block_size, sizeof(uint32_t), 1, packetfile);
        assert(!(block_size % 4));

        block_data = malloc(block_size);
        if(fread(block_data, sizeof(uint8_t), block_size - 12, packetfile) == EOF) {
            printf("EOF reading block data\n");
            this->eof = true;
            return {};
        }

        if(fread(&block_size_spare, sizeof(uint32_t), 1, packetfile) == EOF) {
            printf("EOF reading spare block size\n");
            this->eof = true;
            return {};
        }
        assert(block_size == block_size_spare);

        block_data_8 = (uint8_t *)block_data;
        block_data_32 = (uint32_t *)block_data;

        uint8_t *pc2_telegram;

        switch(block_type) {
            case BLKTP_SECTION_HEADER:
                // just make sure we're not reading with wrong endianity
                assert(block_data_32[0] == 0x1A2B3C4D);
                return {};
            case BLKTP_ENHANCED_PACKET:
                return read_enhanced_packet(block_data, block_size);
            default:
                break;
        }
        free(block_data);
    }
};

int main(int argc, char *argv[]) {
    assert(argc==2);
    PcapngReader r(argv[1]);
    std::vector<uint8_t> reassembly_buffer;
    unsigned int bytes_left_to_reassemble;
    while (!r.eof) {
        auto packet = r.get_next_packet();

        if ( packet.size() ) {

            // is this a self-contained packet?
            if(!((packet.front() == 0x60) && packet.back() == 0x61)) {
                // Are we reassembling a packet?
                if(reassembly_buffer.size()) {
                    bytes_left_to_reassemble -= packet.size();
                    //printf("%d bytes to reassemble\n", bytes_left_to_reassemble);
                    for (auto x: packet) reassembly_buffer.push_back(x);
                    if (bytes_left_to_reassemble) continue;
                    packet = reassembly_buffer;
                    reassembly_buffer.clear();
                } else {
                    // Will this packet need reassembling?
                    if((packet[2] == 0x00) && (packet[1] >= 8)) {
                        bytes_left_to_reassemble = packet[1] - 4;
                        reassembly_buffer = packet;
                        continue;
                    }
                }
            }

            if((packet[2] == 0x00) || (packet[2] == 0xE0)) {
                if(packet[2] == 0x00) 
                    printf("Incoming -------------------------------------\n");
                else
                    printf("Outgoing -------------------------------------\n");
                printf("Raw: ");
                for(auto foo: packet) {
                    printf("%02X ", foo);
                }
                printf("\n");
                PC2Telegram new_tgram(packet);
                MasterlinkTelegram twogram(new_tgram);
                auto bar = twogram.serialize(); 
                // strip checksum if incoming packet (PC2 does that)
                if(packet[2] == 0x00) {
                    bar.pop_back();
                    bar.pop_back();
                }

                printf("Reserialized: ");
                for(auto foo: bar) {
                    printf("%02X ", foo);
                }
                printf("\n");
                TelegramPrinter::print(twogram);
            } else {
                printf("USB command ----------------------------------\n");
                printf("Raw: ");
                for(auto foo: packet) {
                    printf("%02X ", foo);
                }
                printf("\n");
            }
        }
    }
}
