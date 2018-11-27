#include <libnotify/notify.h>
#include "pc2/pc2.hpp"

class PC2Notifier {
    public:
        PC2Notifier();
        NotifyNotification *volume_notification = nullptr;
        void notify_volume(uint8_t volume);
        void notify_source(std::string source);
};
