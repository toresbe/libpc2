#include <cstdio>
#include <string>
#include <map>
#include <boost/log/trivial.hpp>
#include <vector>
#include <array>
#include <exception>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID 0x0cd4
#define PRODUCT_ID 0x0101

    class eDeviceNotFound: public std::exception { virtual const char* what() const throw() { return "PC2 not found"; } } eDeviceNotFound;

    class PC2USBDeviceFinder {
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

    class PC2USBDeviceException: public std::exception
    {
        virtual const char* what() const throw()
        {
            return "PC2 not found";
        }
    } PC2USBDeviceException;

    typedef std::vector<uint8_t> PC2Message;
    typedef std::vector<uint8_t> PC2Telegram; // a PC2 message with the start, num_bytes and end bytes

    class PC2USBDevice {
        libusb_context *usb_ctx;
        libusb_device *pc2_dev;
        libusb_device_handle *pc2;
        public:

        PC2USBDevice() {
            libusb_init(&this->usb_ctx);
        }

        bool open() {
            try {
                this->pc2_dev = PC2USBDeviceFinder::find_pc2(this->usb_ctx);
            } catch ( std::exception &e ) {
                throw PC2USBDeviceException;
            }
            int result = 0;

            result = libusb_open(this->pc2_dev, &this->pc2);
            if(result) {
                BOOST_LOG_TRIVIAL(error) << "Could not open PC2 device, error:" << result;
                throw PC2USBDeviceException;
            } else {
                BOOST_LOG_TRIVIAL(info) << "Opened PC2 device";
            }
        } 

        bool send_telegram(const PC2Message &message) {
            std::vector<uint8_t> telegram;
            // start of transmission
            telegram.push_back(0x60);
            // nbytes
            telegram.push_back(message.size());
            telegram.insert(telegram.end(), message.begin(), message.end());
            // end of transmission 
            telegram.push_back(0x61);
            printf("Debug: sending telegram ");
            for(auto &x: telegram) {
                printf("%02X ", x);
            }
            printf("\n");
            int actual_length;
            int r = libusb_interrupt_transfer(this->pc2, 0x01, (unsigned char *)telegram.data(), telegram.size(), &actual_length, 0);
            assert(r == 0);
            assert(actual_length == telegram.size());
            return true;
        }

    PC2Telegram get_data(int timeout = 0) {
        uint8_t buffer[512];
        PC2Message msg;
        int i = 0;
        bool eot = false;
        int actual_length;
        while(!eot) {
            int r = libusb_interrupt_transfer(this->pc2, 0x81, buffer, 512, &actual_length, timeout);
            if (r == LIBUSB_ERROR_TIMEOUT) {
                printf("Hit a timeout\n");
            return msg;
            }
            assert(r == 0);
            msg.insert(msg.end(), buffer, buffer+actual_length);
            if(buffer[actual_length - 1] == 0x61) {
                eot = true;
            } else {
                timeout = 1000;
            }
        }
        printf("Got %.2d bytes:", msg.size());
        for(auto c: msg) {
            printf(" %02X", c);
        }
        printf(".\n");
        return msg;
    }
};

class PC2Mixer {
    PC2USBDevice *device;

    public:
    PC2Mixer(PC2USBDevice *device) {
        this->device = device;
    }

    void transmit(bool transmit_enabled) {
        if (transmit_enabled == true) {
            this->device->send_telegram({0xe5, 0x00, 0x01, 0x00, 0x00});
        } else {
            this->device->send_telegram({0xe5, 0x00, 0x00, 0x00, 0x00});
        }
    };
};

class PC2 {
    PC2USBDevice *device;
    PC2Mixer *mixer;

    public:
    PC2() {
        this->device = new PC2USBDevice;        
        this->mixer = new PC2Mixer(this->device);
    }

    void yield() {
        auto foo = this->device->get_data(200);
        if (foo.size()) process_telegram(foo);
    }

    void init() {
        this->device->send_telegram({0xf1});
        yield();
        this->device->send_telegram({0x80, 0x01, 0x00});
        // don't know what any of this means
        yield();
        yield();
        yield();
        this->device->send_telegram({0x29});
        yield();
        this->device->send_telegram({0xf6, 0x10, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00});
        yield();
//        this->device->send_telegram({0xe0, 0x80, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x40, 0x0b, 0x0b, 0x0a, 0x00, 0x03, 0x23, 0x20, 0x45, 0x00, 0x17, 0x04, 0x18, 0x02, 0x76, 0x00});
//        yield();
        this->device->send_telegram({0xe7, 0x01});
        yield();
        this->device->send_telegram({0x26});
        yield();
        this->device->send_telegram({0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00});
        yield();
        this->device->send_telegram({0xe0,0xc0,0x01,0x01,0x0b,0x00,0x00,0x00,0x04,0x01,0x17,0x01,0xea,0x00});
        yield();
        yield();
//        send_beo4_code(0xc0, 0x91);
    }

    bool open() {
        this->device->open();
        init();
        event_loop();

        return true;
    }

    void process_ml_telegram(PC2Telegram & tgram) {
        std::map<uint8_t, std::string> node_map;
        node_map[0x80] = "ALL";
        node_map[0x83] = "ALL";
        node_map[0xC0] = "V_MASTER";
        node_map[0xC1] = "A_MASTER";
        node_map[0x01] = "NODE_01";

        std::map<uint8_t, std::string> tgram_type_map;
        tgram_type_map[0x04] = "Unknown 0x04";
        tgram_type_map[0x04] = "Unknown 0x10";
        tgram_type_map[0x40] = "Clock update";

        std::map<uint8_t, uint8_t> tgram_len_map;
        tgram_len_map[0x04] = 0x0e;
        tgram_len_map[0x10] = 0x0e;

        printf("| Telegram to %s from %s: \n", node_map[tgram[3]].c_str(), node_map[tgram[4]].c_str()); 
        switch(tgram[10]) {
            case(0x40):
                printf("| Time is: %02X:%02X:%02X on 20%02X-%02X-%02X.\n", tgram[16], tgram[17], tgram[18], tgram[22], tgram[21], tgram[20]);
                break;
            case(0x87):
                printf("| Active source is: 0x%02X.\n", tgram[13]);
        }

        printf("| Length: 0x%02x (%s)\n", tgram[1], (tgram[1] == tgram_len_map[tgram[10]] ? "as expected" : "differs from observed so far"));
        printf("| Type: %.15s\n|", tgram_type_map[tgram[10]].c_str());
        for(auto c: tgram) {
            printf(" %02X", c);
        }
        printf(".\n");
    };

    void send_beo4_code(uint8_t dest, uint8_t code) {
        this->device->send_telegram({0x60, 0x12, 0xe0, dest, 0xc1, 0x01, 0x0a, 0x00, 0x47, 0x00, 0x20, 0x05, 0x02, 0x00, 0x01, 0xff, 0xff, code, 0x8a, 0x00, 0x61});
    };
    void process_telegram(PC2Telegram & tgram) {
        if(tgram[2] == 0x00) {
            process_ml_telegram(tgram);
        }
        if(tgram[2] == 0x02) {
            printf("Got remote control code %02x\n", tgram[6]);
            if(tgram[6] == 0x91) {
                this->device->send_telegram({0xfa, 0x38, 0xf0, 0x88, 0x40, 0x00, 0x00});
                yield();
                yield();
                yield();
                this->device->send_telegram({0xe0,0x83,0xc1,0x01,0x14,0x00,0x7a,0x00,0x87,0x1e,0x04,0x7a,0x02,0x00,0x00,0x40, \
                                             0x28,0x01,0x00,0x00,0x00,0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, \
                                             0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x62,0x00,0x00});
                yield();
                yield();
                yield();
                this->device->send_telegram({0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x01,0x01,0xa4,0x00});
                yield();
                yield();
                yield();
                this->device->send_telegram({0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x04,0x03,0x04,0x01,0x02,0x00,0x9b,0x00});
                yield();
                yield();
                yield();
                this->device->send_telegram({0xe0,0xc0,0xc1,0x01,0x0b,0x00,0x00,0x00,0x08,0x00,0x01,0x96,0x00});
                yield();
                yield();
                yield();
                this->device->send_telegram({0xea, 0xff});
                yield();
                yield();
                yield();
                this->device->send_telegram({0xea, 0x81});
                yield();
                yield();
                this->device->send_telegram({0xfa, 0x30, 0xd0, 0x65, 0x80, 0x6c, 0x22});
                yield();
                this->device->send_telegram({0xf9, 0x64});
                yield();
                yield();
                this->device->send_telegram({0xe7, 0x00});
                this->device->send_telegram({0xe3, 0x22, 0x00, 0x00, 0x00});
                this->mixer->transmit(true);
                yield();
                yield();
                this->device->send_telegram({0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x44,0x08,0x05,0x02,0x79,0x00,0x02,0x01,0x00,0x00,0x00,0x65,0x00});
                yield();
                yield();
                this->device->send_telegram({0xe0,0xc0,0xc1,0x01,0x14,0x00,0x00,0x00,0x82,0x0a,0x01,0x06,0x79,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x01,0xa5,0x00});
                yield();
                yield();

            }
        }
    }

    void event_loop() {
        while(1) {
            PC2Telegram telegram;

            telegram = this->device->get_data();
            process_telegram(telegram);
        }
    }


};


int main() {
    PC2 pc;
    pc.open();
}
