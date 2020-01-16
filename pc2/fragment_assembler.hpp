#include <cstdint>
#include <vector>

typedef std::vector<uint8_t> PC2Message;
class PC2MessageFragmentAssembler {
    private:
        unsigned int remaining_bytes;
        std::vector<uint8_t> reassembly_buffer;
        PC2Message assembled_message;
    public:
        void operator<< (const PC2Message &fragment);
        bool has_complete_message();
        PC2Message get_message();
};
