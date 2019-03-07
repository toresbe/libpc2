#ifndef __PC2_HPP
#define __PC2_HPP

#include <future>
#include <functional>
#include <vector>
#include <cstdint>
#include <ctime>
#include <map>

class PC2;
class PC2Beolink;
#include "pc2/beo4.hpp"
namespace Beolink {
    class Source {
        public:
        /*! send keystroke to source */
        virtual void keypress(Beo4::keycode code) = 0;
        /*! start media playback/request source via beolink */
        virtual void start() = 0;
        /*! end media playback/unsubscribe from beolink */
        virtual void stop() = 0; 
    };

    class LocalSource: public Source {
    };

    class BeolinkSource: public Source {
        PC2Beolink * beolink;
    };

    class LocalStreamingSource: public LocalSource {
    };

    class LocalSeekableSource: public LocalSource {
    };

    typedef std::pair<Masterlink::source, LocalSource *> LocalSourceList;
}

#include "ml/masterlink.hpp"
#include "ml/telegram.hpp"
#include "pc2/pc2device.hpp"
#include "pc2/pc2interface.hpp"
#include "pc2/mixer.hpp"

#define ML_STATE_STOPPED 0x01
#define ML_STATE_PLAYING 0x02
#define ML_STATE_FFWD    0x03
#define ML_STATE_REW     0x04


typedef std::pair<std::shared_ptr<MasterlinkTelegram>, std::shared_ptr<std::promise<MasterlinkTelegram>>> pending_request_t;
typedef std::list<pending_request_t *> pending_request_queue_t;

class TelegramRequestTimeout: public std::exception {};

/**! Beolink-related functions for communication and audio bus arbitration
 */
class PC2Beolink {
    PC2 *pc2;
    public:
    pending_request_queue_t pending_request_queue;
    void handle_ml_request(MasterlinkTelegram & mlt);
    void handle_ml_status(MasterlinkTelegram & mlt);
    std::shared_future<MasterlinkTelegram> send_request(std::shared_ptr<MasterlinkTelegram> tgram);
    MasterlinkTelegram interrogate(MasterlinkTelegram &tgram);
    void send_telegram(MasterlinkTelegram &tgram);
    void yield();
    void yield(std::string description);
    void broadcast_timestamp();
    void request_source(uint8_t source_id);
    void send_beo4_code(uint8_t dest, uint8_t code);
    void process_beo4_keycode(uint8_t type, uint8_t keycode);
//    void send_audio();
    void process_ml_telegram(PC2Telegram & tgram);
};

class PC2 {
    uint8_t active_source = 0;
    unsigned int listener_count = 0;
    std::time_t last_light_timestamp = 0;
    void init();
    bool open();

    public:
    // TODO: make private
    PC2Interface *interface;
    std::function<void(Beo4::keycode)> keystroke_callback = nullptr;
    PC2Device *device;
    PC2Mixer *mixer;
    PC2Beolink *beolink;

    PC2(PC2Interface * interface);
    ~PC2();

    void event_loop(volatile bool & keepRunning);
    void keypress(Beo4::keycode keycode);
};

class LightControlHandler {
    public: 
    virtual void handle_command(Beo4::keycode code) = 0;
};

/**! \brief Media core class
 */
class MediaCore {
    Beolink::Source * SourceFactory(Masterlink::source source);
    LightControlHandler * lc_handler;
    PC2 * dev;
    public:
    Beolink::Source * current_source;
    void set_source(Beolink::Source *new_source);
    void keypress(Beo4::keycode keycode);
    void standby();
};  

#endif
