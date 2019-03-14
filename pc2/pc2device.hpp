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

/**! \brief Low-level USB I/O with PC2 device
 * 
 * Implements only very basic read and write methods.
 */
class PC2DeviceIO {
    libusb_device *pc2_dev;
    libusb_device_handle *pc2_handle;
    libusb_transfer *transfer_out = NULL;
    libusb_transfer *transfer_in = NULL;

    std::mutex transfer_out_mutex;

    std::vector<uint8_t> reassembly_buffer;
    unsigned int remaining_bytes;
    uint8_t input_buffer[1024];
    std::mutex mutex;
    std::condition_variable data_waiting;
    std::queue<PC2Telegram> inbox;
    void send_next();
    std::thread * usb_thread;
    public:
    void usb_loop();
    libusb_context *usb_ctx;
    bool open();
    static void read_callback(struct libusb_transfer *transfer);
    static void write_callback(struct libusb_transfer *transfer);
    PC2DeviceIO();
    ~PC2DeviceIO();
    bool write(const PC2Message &message);
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
    std::queue<PC2Telegram> inbox;
    void process_message(PC2Telegram & tgram);
    void send_message(const PC2Message &message);
    void set_address_filter(PC2Interface::address_mask_t mask);
    PC2Device(PC2* pc2);
    bool open();
    bool close();
};

#endif
