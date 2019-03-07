// Code for the PC2 USB device
#include "pc2/pc2device.hpp"
#include "ml/telegram.hpp"
#include "ml/pprint.hpp"

#include <boost/log/trivial.hpp>
#include <iostream>
#include <boost/format.hpp>

#define VENDOR_ID 0x0cd4
#define PRODUCT_ID 0x0101

class eDeviceNotFound : public std::exception { virtual const char* what() const throw() { return "PC2 not found"; } } eDeviceNotFound;
class eDeviceError : public std::exception { virtual const char* what() const throw() { return "Failed to open PC2"; } } eDeviceError;

class PC2DeviceIOFinder {
    public:
        static libusb_device * find_pc2(libusb_context * ctx) {
            // cycle through USB devices, and return first matching PC2 device.
            libusb_device **device_list;
            ssize_t cnt = libusb_get_device_list(ctx, &device_list);
            ssize_t i = 0;
            if (cnt < 0)
                return nullptr;
            for (i = 0; i < cnt; i++) {
                if (is_pc2(device_list[i])) {
                    BOOST_LOG_TRIVIAL(info) << "Found PC2 device";
                    return device_list[i];
                }
            }
            BOOST_LOG_TRIVIAL(warning) << "Could not find PC2 device.";
            throw eDeviceNotFound;
        }

    private:
        static bool is_pc2(libusb_device * device) {
            // Compare the USB device descriptor with the defined VENDOR_ID and PRODUCT_ID.
            // Return true if 'device' points to a PC2.
            struct libusb_device_descriptor desc;
            libusb_get_device_descriptor(device, &desc);
            if ((desc.idVendor == VENDOR_ID) && (desc.idProduct == PRODUCT_ID))
                return true;
            return false;
        }
};


PC2DeviceIO::PC2DeviceIO() {
    libusb_init(&this->usb_ctx);
}

bool PC2DeviceIO::open() {
    try {
        this->pc2_dev = PC2DeviceIOFinder::find_pc2(this->usb_ctx);
    }
    catch (std::exception &e) {
        throw eDeviceError;
    }
    int result = 0;

    result = libusb_open(this->pc2_dev, &this->pc2_handle);
    if (result) {
        BOOST_LOG_TRIVIAL(error) << "Could not open PC2 device, error:" << result;
        throw eDeviceError;
    }
    else {
        BOOST_LOG_TRIVIAL(info) << "Opened PC2 device";
        return true;
    }
}

bool PC2DeviceIO::write(const PC2Message &message) {
    std::vector<uint8_t> telegram; //(message.size() + 2);
    // start of transmission
    telegram.push_back(0x60);
    // nbytes
    telegram.push_back(message.size());
    telegram.insert(telegram.end(), message.begin(), message.end());
    // end of transmission
    telegram.push_back(0x61);

    std::string debug_message = "Sent:";

    for (auto &x : telegram) {
        debug_message.append(boost::str(boost::format(" %02X") % (unsigned int)x));
    }

    BOOST_LOG_TRIVIAL(debug) << debug_message;

    // if we're asked to send a telegram, print a debug representation of it
    if(telegram[2] == 0xe0) {
        MasterlinkTelegram foo = telegram;
        TelegramPrinter::print(foo);
    }
    int actual_length;
    int r = libusb_interrupt_transfer(this->pc2_handle, 0x01, (unsigned char *)telegram.data(), telegram.size(), &actual_length, 0);
    assert(r == 0);
    assert(actual_length >= 0);
    assert((unsigned int)actual_length == telegram.size());
    return true;
}

PC2Telegram PC2DeviceIO::read(int timeout) {
    // in some cases, a 60 ... 61 message can occur _inside_ a multi-part message,
    // so FIXME: prepare for that eventuality!
    uint8_t buffer[512];
    PC2Message msg;
    bool eot = false;
    int actual_length;
    while (!eot) {
        int r = libusb_interrupt_transfer(this->pc2_handle, 0x81, buffer, 512, &actual_length, timeout);
        if (r == LIBUSB_ERROR_TIMEOUT) {
            //BOOST_LOG_TRIVIAL(info) << "Timed out waiting for message";
            return msg;
        }
        assert(r == 0);
        msg.insert(msg.end(), buffer, buffer + actual_length);
        if (buffer[actual_length - 1] == 0x61) {
            eot = true;
        }
        else {
            timeout = 1000;
        }
    }

    std::string debug_message = " Got:";
    for (auto &x : msg) {
        debug_message.append(boost::str(boost::format(" %02X") % (unsigned int)x));
    }
    BOOST_LOG_TRIVIAL(debug) << debug_message;

    return msg;
}

bool PC2Device::open() {
    this->usb_device.open();
};

void PC2Device::send_message(const PC2Message &message) {
    this->usb_device.write(message);
}

bool PC2Device::close() {
    this->usb_device.write({0xa7});
}

PC2Device::PC2Device(PC2* pc2) {
    this->pc2 = pc2;
}
