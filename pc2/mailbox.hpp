#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

typedef std::vector<uint8_t> PC2Message;

class PC2Mailbox {
    private:
        std::mutex msg_available_mutex;         ///< message waiting condition variable mutex
        std::condition_variable msg_available;  ///< message waiting condition variable
        std::mutex queue_mutex;                 ///< inbox mutex
        std::queue<PC2Message> queue;
    public:
        int count();
        PC2Message pop_sync();
        void push(PC2Message msg);
};
