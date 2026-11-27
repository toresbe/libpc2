#ifndef __PC2DEVICE_HPP
#define __PC2DEVICE_HPP

#include <vector>
#include <thread>
#include <future>
#include <exception>
#include <queue>

#include "pc2/pc2_usb_io.hpp"

class PC2Device;
#include "pc2/pc2interface.hpp"

#include "pc2/fragment_assembler.hpp"
#include "pc2/mailbox.hpp"
#include "pc2/telegram_pacer.hpp"

/**! \brief Higher-level PC2 I/O
 */
class PC2Device {
    PC2DeviceIO usb_device;
    PC2 * pc2;
    PC2TelegramPacer ml_pacer;              ///< Paces Masterlink (0xE0) telegrams on the PC2's status answers
    std::thread * event_thread;
    void event_loop();
    public:
    void init();
    PC2MessageFragmentAssembler message_assembler; ///< Message fragment assembler
    PC2Mailbox inbox;
    void process_message(const PC2Message & tgram);
    void send_message(const PC2Message &message);
    void send_option(PC2Interface::address_mask_t mask);   ///< F6 "option" telegram (BeoIpcML: TRANSMIT OPTION)
    PC2Device(PC2* pc2);
    void open();
    void close();
};

#endif
