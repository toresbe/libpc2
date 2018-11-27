#ifndef __PC2_HPP
#define __PC2_HPP

#include <vector>
#include <cstdint>
#include <ctime>
#include <map>

#include "pc2/pc2device.hpp"
#include "pc2/notify.hpp"
#include "pc2/mixer.hpp"
#include "amqp/amqp.hpp"


// Only using defines because I don't understand static const yet
#define ML_SRC_PC 0x47
class PC2 {
    PC2USBDevice *device;
    AMQP *amqp = nullptr;
    uint8_t active_source = 0;
    unsigned int listener_count = 0;
    std::map<uint8_t, std::string> source_name;
    std::time_t last_light_timestamp = 0;

    public:
    PC2Notifier *notify = nullptr;
    PC2Mixer *mixer;
    PC2();
    void yield();
    void yield(std::string description);
    void set_address_filter();
    void broadcast_timestamp();
    ~PC2();
    void init();
    bool open();
    void process_ml_telegram(PC2Telegram & tgram);
    void send_beo4_code(uint8_t dest, uint8_t code);
    void process_beo4_keycode(uint8_t keycode);
    void process_telegram(PC2Telegram & tgram);
    void expect_ack();
    void send_audio();
    void event_loop(volatile bool & keepRunning);
};

#endif
