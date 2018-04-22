#ifndef __PC2DEVICE_HPP
#define __PC2DEVICE_HPP

#include "pc2/pc2.hpp"
#include <vector>
#include <exception>
#include <libusb-1.0/libusb.h>

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