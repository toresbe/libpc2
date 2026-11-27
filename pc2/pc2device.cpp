// Code for the PC2 USB device
#include "pc2/pc2device.hpp"
#include "masterlink/telegram.hpp"
#include "masterlink/pprint.hpp"

#include <boost/log/trivial.hpp>
#include <iostream>
#include <chrono>
#include <future>

/** \brief Initializes the device.
 *
 *  B&O's own software (BeoIpcML.exe) logs each step: "ML START" sends 80 <node address> 00,
 *  "GET VERSION" sends 29, and "TRANSMIT OPTION" sends the F6 option telegram for its
 *  SystemConfiguration (see send_option()). We send F1, 80 and F6 without waiting for the version
 *  reply; 29, E7 01 and 26 are not reproduced yet.
 */
void PC2Device::init() {
    // As BeoIpcML.exe does it (static analysis, see reports/pc2-control.md):
    //   F1; "80 <node address> 00" (the address comes from the registry value LastLegalAdr, default
    //   0x29, and is replaced by the address the PC2 reports in its ML-status replies); then, after
    //   the version reply, "F6 <option>" chosen by SystemConfiguration (see send_option()).
    this->send_message({ 0xf1 });
    uint8_t address = this->pc2->interface->ml_node_address;
    if (!address) {
        switch (this->pc2->interface->address_mask) {
            case PC2Interface::address_mask_t::beoport: address = 0x40; break;
            case PC2Interface::address_mask_t::source_center: address = 0x29; break;
            default: address = 0x01; break;
        }
        this->pc2->interface->ml_node_address = address;
    }
    this->send_message({ 0x80, address, 0x00 });
    this->send_option(this->pc2->interface->address_mask);
}

void PC2Device::open() {
    this->usb_device.open();
    this->event_thread = new std::thread (&PC2Device::event_loop, this);
};

void PC2Device::event_loop() {
    BOOST_LOG_TRIVIAL(info) << "PC2Device event thread running...";
    while(1) {
        auto message_future = this->usb_device.read();
        PC2Message fragment = message_future.get();

        // Raw wake-up packets (no 60 ... 61 framing), only between messages
        if (!message_assembler.in_progress()) {
            PC2Wakeup wakeup = pc2_wakeup_packet(fragment);
            if (wakeup != PC2Wakeup::none) {
                bool by_remote = (wakeup == PC2Wakeup::by_remote);
                BOOST_LOG_TRIVIAL(info) << (by_remote ? "PC2 woke the PC (remote key)" : "PC2: USB bus resumed");
                this->pc2->interface->pc2_wakeup(by_remote);
                continue;
            }
        }

        try {
            message_assembler << fragment;
        } catch (const std::runtime_error &e) {
            // A malformed packet used to end the event thread (and the program); log and go on
            BOOST_LOG_TRIVIAL(warning) << "Dropping malformed PC2 packet: " << e.what();
            continue;
        }
        if(message_assembler.has_complete_message()) {
            PC2Message message = message_assembler.get_message();
            // Type 0x01 is the PC2's status answer to a Masterlink telegram we sent
            if (message.size() > 2 && message[2] == 0x01)
                this->ml_pacer.acknowledge();
            // ML-status (object 01, e.g. "60 04 41 01 01 02 61"): byte 5 is our node address;
            // BeoIpcML adopts it when it is 1..0x7f
            if (message.size() >= 7 && (message[2] & 0x1f) == 0x01 && message[5] >= 0x01 && message[5] <= 0x7f
                    && message[5] != this->pc2->interface->ml_node_address) {
                BOOST_LOG_TRIVIAL(info) << "PC2 reports Masterlink node address " << (int)message[5];
                this->pc2->interface->ml_node_address = message[5];
            }
            inbox.push(message);
        }
    }
}

/** \brief Send a message to the PC2.
 *  Masterlink telegrams (first byte 0xE0) are queued and sent one at a time, each after the PC2
 *  has answered the previous one, as B&O's software does. Everything else goes out immediately.
 */
void PC2Device::send_message(const PC2Message &message) {
    if (!message.empty() && message[0] == 0xe0)
        this->ml_pacer.enqueue(message);
    else
        this->usb_device.write(message);
}

void PC2Device::close() {
    this->usb_device.write({ 0xa7 });
}

PC2Device::PC2Device(PC2* pc2)
    : ml_pacer([this](const PC2Message &telegram) { this->usb_device.write(telegram); }) {
    this->pc2 = pc2;
}

void PC2Device::send_option(PC2Interface::address_mask_t address_mask) {
    // BeoIpcML picks the F6 payload from its SystemConfiguration (registry), 1..10:
    //   1: 02 C0 C1 80                 2: 01 C0 80 83               3: 00 82 80 83
    //   4/5/6/8: 10 C1 80 83 <cfg> R C   7/9/10: 10 80 80 83 <cfg> R C
    // where R = LCMLRerouteON and C = CompatabilityEnabled (registry, normally 0). Its own log names
    // 4 "A.OPT=1 (A-master)", 5 "A.OPT=0 (A-master)", 6 "A.OPT=2 (AV-master)", 7 "SL-source center",
    // 8 "Compatability mode", 9 "A.OPT=5 (Linkroom A-master)", 10 "A.OPT=6 (Linkroom AV-master)".
    switch(this->pc2->interface->address_mask) {
        case PC2Interface::address_mask_t::audio_master:
            BOOST_LOG_TRIVIAL(info) << "Sending option: audio master mode";
            this->pc2->device->send_message({ 0xf6, 0x10, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
            return;
        case PC2Interface::address_mask_t::beoport:
            BOOST_LOG_TRIVIAL(info) << "Sending option: Beoport PC2 mode";
            this->pc2->device->send_message({ 0xf6, 0x00, 0x82, 0x80, 0x83 });
            return;
        case PC2Interface::address_mask_t::source_center:
            BOOST_LOG_TRIVIAL(info) << "Sending option: source center mode (SystemConfiguration 7)";
            this->pc2->device->send_message({ 0xf6, 0x10, 0x80, 0x80, 0x83, 0x07, 0x00, 0x00 });
            return;
        case PC2Interface::address_mask_t::promisc:
            BOOST_LOG_TRIVIAL(info) << "Sending option: promiscuous mode";
            this->pc2->device->send_message({ 0xf6, 0xc0, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
            return;
        default:
            BOOST_LOG_TRIVIAL(error) << "Did not get address mask!";
    }
}
