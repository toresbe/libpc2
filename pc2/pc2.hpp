#ifndef __PC2_HPP
#define __PC2_HPP

#include <vector>
#include <cstdint>
#include <ctime>
#include <map>

#include "pc2/pc2device.hpp"
#include "pc2/mixer.hpp"

#define ML_STATE_STOPPED 0x01
#define ML_STATE_PLAYING 0x02
#define ML_STATE_FFWD    0x03
#define ML_STATE_REW     0x04


// Only using defines because I don't understand static const yet
#define ML_SRC_DOORCAM 0x3E
#define ML_SRC_V_AUX2 0x3E
#define ML_SRC_TV 0x0B
#define ML_SRC_V_MEM 0x15
#define ML_SRC_V_TAPE 0x15
#define ML_SRC_DVD_2 0x16
#define ML_SRC_V_TAPE2 0x16
#define ML_SRC_PHONO 0xa1
#define ML_SRC_CD 0x8d
#define ML_SRC_A_AUX 0x97
#define ML_SRC_PC 0x47

class PC2; 

class PC2Interface {
    public:
    PC2 * pc2;
    virtual void beo4_press(uint8_t keycode) = 0;
};

class PC2 {
    PC2USBDevice *device;
    uint8_t active_source = 0;
    unsigned int listener_count = 0;
    std::map<uint8_t, std::string> source_name;
    std::time_t last_light_timestamp = 0;
    PC2Interface *interface;

    public:
    PC2Mixer *mixer;
    PC2(PC2Interface * interface);
    void yield();
    void yield(std::string description);
    void set_address_filter();
    void request_source(uint8_t source_id);
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
