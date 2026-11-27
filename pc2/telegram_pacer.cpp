#include "pc2/telegram_pacer.hpp"

#include <boost/log/trivial.hpp>

PC2TelegramPacer::PC2TelegramPacer(send_function_t send, std::chrono::milliseconds timeout)
    : send(send), timeout(timeout) {
    this->worker = std::thread(&PC2TelegramPacer::run, this);
}

PC2TelegramPacer::~PC2TelegramPacer() {
    {
        std::scoped_lock<std::mutex> lk(this->mutex);
        this->stopping = true;
    }
    this->cv.notify_all();
    this->worker.join();
}

void PC2TelegramPacer::enqueue(const PC2Message &telegram) {
    {
        std::scoped_lock<std::mutex> lk(this->mutex);
        this->queue.push_back(telegram);
    }
    this->cv.notify_all();
}

void PC2TelegramPacer::acknowledge() {
    {
        std::scoped_lock<std::mutex> lk(this->mutex);
        if (!this->awaiting_ack) {
            BOOST_LOG_TRIVIAL(debug) << "Masterlink status received with no telegram outstanding";
            return;
        }
        this->awaiting_ack = false;
    }
    this->cv.notify_all();
}

size_t PC2TelegramPacer::pending() {
    std::scoped_lock<std::mutex> lk(this->mutex);
    return this->queue.size();
}

unsigned int PC2TelegramPacer::timeouts() {
    std::scoped_lock<std::mutex> lk(this->mutex);
    return this->timeout_count;
}

void PC2TelegramPacer::run() {
    std::unique_lock<std::mutex> lk(this->mutex);
    while (true) {
        this->cv.wait(lk, [this] { return this->stopping || !this->queue.empty(); });
        if (this->stopping) return;

        PC2Message telegram = this->queue.front();
        this->queue.pop_front();
        // Set before sending, so an answer that arrives before send() returns is not missed.
        this->awaiting_ack = true;

        lk.unlock();
        this->send(telegram);
        lk.lock();

        bool answered = this->cv.wait_for(lk, this->timeout,
                [this] { return this->stopping || !this->awaiting_ack; });
        if (this->stopping) return;
        if (!answered) {
            this->awaiting_ack = false;
            this->timeout_count++;
            BOOST_LOG_TRIVIAL(warning) << "No Masterlink status from PC2 within "
                << this->timeout.count() << " ms; sending next telegram anyway";
        }
    }
}
