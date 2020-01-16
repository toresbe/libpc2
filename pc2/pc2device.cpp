// Code for the PC2 USB device
#include "pc2/pc2device.hpp"
#include "masterlink/telegram.hpp"
#include "masterlink/pprint.hpp"

#include <boost/log/trivial.hpp>
#include <iostream>
#include <chrono>
#include <future>

/** \brief Initializes the device.
 *  \todo More research is needed into the significance of each instruction and its data.
 */
void PC2Device::init() {
    this->send_message({ 0xf1 });
    // TODO: Move this to separate function
    this->send_message({ 0x80, 0x01, 0x00 });
}

void PC2Device::open() {
    this->usb_device.open();
    this->event_thread = new std::thread (&PC2Device::event_loop, this);
};

void PC2Device::event_loop() {
    BOOST_LOG_TRIVIAL(info) << "PC2Device event thread running...";
    while(1) {
        auto message_future = this->usb_device.read();
        message_assembler << message_future.get();
        if(message_assembler.has_complete_message()) {
            BOOST_LOG_TRIVIAL(info) << "Got telegram; signalling semaphore";
            inbox.push(message_assembler.get_message());
        }
    }
}

// FIXME: Is this really necessary?
void PC2Device::send_message(const PC2Message &message) {
    this->usb_device.write(message);
}

void PC2Device::close() {
    this->usb_device.write({ 0xa7 });
}

PC2Device::PC2Device(PC2* pc2) {
    this->pc2 = pc2;
}

void PC2Device::set_address_filter(PC2Interface::address_mask_t address_mask) {
    // I am not at all sure how this works but it seems to be a variable-length list of destination
    // address bytes to which the PC2 should respond
    switch(this->pc2->interface->address_mask) {
        case PC2Interface::address_mask_t::audio_master:
            BOOST_LOG_TRIVIAL(info) << "Setting address filter to audio master mode";
            this->pc2->device->send_message({ 0xf6, 0x10, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
            return;
        case PC2Interface::address_mask_t::beoport:
            BOOST_LOG_TRIVIAL(info) << "Setting address filter to Beoport PC2 mode";
            this->pc2->device->send_message({ 0xf6, 0x00, 0x82, 0x80, 0x83 });
            return;
        case PC2Interface::address_mask_t::promisc:
            BOOST_LOG_TRIVIAL(info) << "Setting address filter to promiscuous mode";
            this->pc2->device->send_message({ 0xf6, 0xc0, 0xc1, 0x80, 0x83, 0x05, 0x00, 0x00 });
            return;
        default:
            BOOST_LOG_TRIVIAL(error) << "Did not get address mask!";
    }
}
