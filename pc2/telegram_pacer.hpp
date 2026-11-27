#ifndef __PC2_TELEGRAM_PACER_HPP
#define __PC2_TELEGRAM_PACER_HPP

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

typedef std::vector<uint8_t> PC2Message;

/**! \brief Stop-and-wait sender for Masterlink telegrams (PC2 messages starting with 0xE0)
 *
 * The PC2 answers every Masterlink telegram it is given with a status message (type 0x01,
 * e.g. "60 04 01 01 01 02 61"). In USB captures of B&O's own software (BeoIpcML), the next
 * telegram is never sent before that answer has arrived. This class reproduces that: queued
 * telegrams are handed to the send function one at a time, and the next one goes out when
 * acknowledge() is called or, if no answer comes, after the timeout.
 */
class PC2TelegramPacer {
    public:
        typedef std::function<void(const PC2Message &)> send_function_t;

        PC2TelegramPacer(send_function_t send,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
        ~PC2TelegramPacer();

        /*! Queue a telegram; it is sent once every earlier telegram has been acknowledged. */
        void enqueue(const PC2Message &telegram);
        /*! Call when the PC2 reports Masterlink status (message type 0x01). */
        void acknowledge();
        /*! Number of telegrams not yet sent. */
        size_t pending();
        /*! Number of telegrams whose acknowledgement timed out (for diagnostics/tests). */
        unsigned int timeouts();

    private:
        void run();

        send_function_t send;
        std::chrono::milliseconds timeout;
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<PC2Message> queue;
        bool awaiting_ack = false;
        bool stopping = false;
        unsigned int timeout_count = 0;
        std::thread worker;
};

#endif
