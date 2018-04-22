#ifndef __PC2DEVICE_HPP
#define __PC2DEVICE_HPP

#include <vector>
#include <exception>
#include <libusb-1.0/libusb.h>
typedef std::vector<uint8_t> PC2Message;
typedef std::vector<uint8_t> PC2Telegram; // a PC2 message with the start, num_bytes and end bytes
class PC2USBDevice {
	libusb_context *usb_ctx;
	libusb_device *pc2_dev;
	libusb_device_handle *pc2;
public:
	PC2USBDevice();
	bool open();
	bool send_telegram(const PC2Message &message);
	PC2Telegram get_data(int timeout = 0);
};

#endif