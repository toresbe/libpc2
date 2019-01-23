#ifndef __PC2_HPP
#define __PC2_HPP

#include <future>
#include <vector>
#include <cstdint>
#include <ctime>
#include <map>

#include "pc2/pc2device.hpp"
#include "pc2/mixer.hpp"
#include "ml/telegram.hpp"

#define ML_STATE_STOPPED 0x01
#define ML_STATE_PLAYING 0x02
#define ML_STATE_FFWD    0x03
#define ML_STATE_REW     0x04

class PC2;

//typedef struct SourceInterface {} SourceInterface;
//typedef std::pair<Masterlink::source, SourceInterface> 1
class PC2Interface {
    public:
    PC2 * pc2;
    enum address_masks {
        audio_master,
        beoport,
        promisc
    };
    int address_mask;
    virtual void beo4_press(uint8_t keycode) = 0;
};

typedef std::pair<std::shared_ptr<MasterlinkTelegram>, std::shared_ptr<std::promise<MasterlinkTelegram>>> pending_request_t;
typedef std::list<pending_request_t *> pending_request_queue_t;

class TelegramRequestTimeout: public std::exception {};

class PC2 {
    uint8_t active_source = 0;
    unsigned int listener_count = 0;
    std::time_t last_light_timestamp = 0;
    PC2Interface *interface;

    //TODO: move into separate PC2Datanet or similar
    pending_request_queue_t pending_request_queue;
    public:
    PC2USBDevice *device;
    PC2Mixer *mixer;
    PC2(PC2Interface * interface);
    void send_telegram(MasterlinkTelegram &tgram);
    void yield();
    void yield(std::string description);
    void set_address_filter();
    void request_source(uint8_t source_id);
    void broadcast_timestamp();
    ~PC2();
    void init();
    bool open();

    // TODO: Move into more suitable class
    void process_ml_telegram(PC2Telegram & tgram);
    void handle_ml_request(MasterlinkTelegram & mlt);
    void handle_ml_status(MasterlinkTelegram & mlt);
    void process_telegram(PC2Telegram & tgram);
    std::shared_future<MasterlinkTelegram> send_request(std::shared_ptr<MasterlinkTelegram> tgram);
    MasterlinkTelegram interrogate(MasterlinkTelegram &tgram);
    void expect_ack();

    void send_beo4_code(uint8_t dest, uint8_t code);
    void process_beo4_keycode(uint8_t type, uint8_t keycode);

    void send_audio();
    void event_loop(volatile bool & keepRunning);
};

#endif
