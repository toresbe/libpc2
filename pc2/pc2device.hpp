#ifndef __PC2DEVICE_HPP
#define __PC2DEVICE_HPP

#include <vector>
#include <thread>
#include <future>
#include <exception>
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
    libusb_context *usb_ctx;
    libusb_device *pc2_dev;
    libusb_device_handle *pc2_handle;
    public:
    bool open();
    PC2DeviceIO();
    ~PC2DeviceIO();
    bool write(const PC2Message &message);
    PC2Telegram read(int timeout = 0);
};

/**! \brief Higher-level PC2 I/O
 */
class PC2Device {
    PC2DeviceIO usb_device;
    PC2 * pc2;
    public:
    void process_message(PC2Telegram & tgram);
    void send_message(const PC2Message &message);
    void set_address_filter(PC2Interface::address_mask_t mask);
    PC2Device(PC2* pc2);
    bool open();
    bool close();
};

#endif
