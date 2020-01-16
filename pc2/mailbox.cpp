#include "pc2/mailbox.hpp"

int PC2Mailbox::count() {
    return queue.size();
}

void PC2Mailbox::push(PC2Message msg) {
    { 
        std::scoped_lock<std::mutex> lk(queue_mutex);
        queue.push(msg);
    }
    { 
        std::scoped_lock<std::mutex> lk(msg_available_mutex);
        msg_available.notify_one();
    }
}

PC2Message PC2Mailbox::pop_sync() {
    // If a message is waiting to be read, return it immediately
    {
        std::scoped_lock<std::mutex> lk(queue_mutex);
        if(queue.size()) {
            auto msg = queue.front();
            queue.pop();
            return msg;
        }
    }
    // (note that the lock goes out of scope and is deleted))
    // Nope, there's not, so we block until the condition variable
    // is notified that there is
    std::unique_lock<std::mutex> cv_lk(msg_available_mutex);
    msg_available.wait(cv_lk, [this] {return queue.size();});
    cv_lk.unlock();

    std::scoped_lock<std::mutex> lk(queue_mutex);
    auto msg = queue.front();
    queue.pop();
    return msg;
}
