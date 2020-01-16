#ifndef __PC2DEVICE_HPP
#define __PC2DEVICE_HPP

#include <vector>
#include <thread>
#include <future>
#include <exception>
#include <queue>
#include <libusb-1.0/libusb.h>


typedef std::vector<uint8_t> PC2Message;
typedef std::vector<uint8_t> PC2Telegram; // a PC2 message with the start, num_bytes and end bytes
class PC2Device;
#include "pc2/pc2interface.hpp"

#include "pc2/fragment_assembler.hpp"
#include "pc2/mailbox.hpp"

/**! \brief Low-level USB I/O with PC2 device
 * 
 * Implements only very basic read and write methods.
 */
class PC2DeviceIO {
    libusb_context *usb_ctx;                ///< libusb context variable
    libusb_device *pc2_dev;                 ///< Reference to PC2 device
    libusb_device_handle *pc2_handle;       ///< File descriptor/device handle of PC2 device

    libusb_transfer *transfer_out = NULL;   ///< Outgoing transfer struct
    std::timed_mutex transfer_out_mutex;    ///< Mutex for outgoing transfer struct

    libusb_transfer *transfer_in = NULL;    ///< Incoming transfer struct
    uint8_t input_buffer[1024];             ///< Incoming transfer data buffer

    bool keep_running = true;               ///< If this is set to 0, the USB event thread will terminate after libusb_handle_event returns

    std::thread * usb_thread;               ///< USB event handling loop thread
    PC2MessageFragmentAssembler message_assembler; ///< Message fragment assembler

    void send_next();
    void usb_loop();

    public:
    std::shared_ptr<PC2Mailbox> inbox;
    void open();
    static void read_callback(struct libusb_transfer *transfer);
    static void write_callback(struct libusb_transfer *transfer);
    //TODO: Replace with the following
    //static void read_callback(struct libusb_transfer *transfer, PC2DeviceIO * instance);
    //static void write_callback(struct libusb_transfer *transfer, PC2DeviceIO * instance);
    PC2DeviceIO();
    ~PC2DeviceIO();
    void write(const PC2Message &message);
    PC2Telegram read();
};

/**! \brief Higher-level PC2 I/O
 */
class PC2Device {
    PC2DeviceIO usb_device;
    PC2 * pc2;
    std::thread * event_thread;
    void event_loop();
    public:
    void init();
    std::shared_ptr<PC2Mailbox> inbox;
    void process_message(const PC2Telegram & tgram);
    void send_message(const PC2Message &message);
    void set_address_filter(PC2Interface::address_mask_t mask);
    PC2Device(PC2* pc2);
    void open();
    void close();
};

#endif
