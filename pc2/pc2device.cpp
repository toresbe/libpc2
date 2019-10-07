// Code for the PC2 USB device
#include "pc2/pc2device.hpp"
#include "ml/telegram.hpp"
#include "ml/pprint.hpp"

#include <boost/log/trivial.hpp>
#include <iostream>
#include <chrono>
#include <boost/format.hpp>

#define VENDOR_ID 0x0cd4
#define PRODUCT_ID 0x0101
#define USB_ENDPOINT_IN     (LIBUSB_ENDPOINT_IN  | 1)   /* endpoint address */
#define USB_ENDPOINT_OUT    (LIBUSB_ENDPOINT_OUT | 2)   /* endpoint address */

class eDeviceNotFound : public std::exception { virtual const char* what() const throw() { return "PC2 not found"; } } eDeviceNotFound;
class eDeviceError : public std::exception { virtual const char* what() const throw() { return "Failed to open PC2"; } } eDeviceError;

PC2DeviceIO *singleton = nullptr;

/**! \brief Simple class to locate the PC2 device on the USB bus
 */
class PC2DeviceIOFinder {
    public:
        /**! \brief Search USB bus for PC2 vendor and product ID and return libusb_device if found
         */
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
        /**! \brief Compare the USB device descriptor with the defined VENDOR_ID and PRODUCT_ID.
        /    \return true if 'device' points to a PC2.
        */
        static bool is_pc2(libusb_device * device) {
            struct libusb_device_descriptor desc;
            libusb_get_device_descriptor(device, &desc);
            if ((desc.idVendor == VENDOR_ID) && (desc.idProduct == PRODUCT_ID))
                return true;
            return false;
        }
};


/** \brief Starts event-handling thread and registers singleton
 */
PC2DeviceIO::PC2DeviceIO() {
    libusb_init(&this->usb_ctx);
    this->usb_thread = new std::thread (&PC2DeviceIO::usb_loop, this);
    singleton = this;
}

/** \brief Initializes the device.
 *  \todo More research is needed into the significance of each instruction and its data.
 */
void PC2Device::init() {
    this->send_message({ 0xf1 });
    // TODO: Move this to separate function
    this->send_message({ 0x80, 0x01, 0x00 });
}

/** \brief Attempt to open USB device and allocate transfer data structures
*/
void PC2DeviceIO::open() {
    libusb_device *pc2_dev;
    try {
        pc2_dev = PC2DeviceIOFinder::find_pc2(this->usb_ctx);
    }
    catch (std::exception &e) {
        throw eDeviceError;
    }
    int result = 0;


    result = libusb_open(pc2_dev, &this->pc2_handle);
    if (result) {
        BOOST_LOG_TRIVIAL(error) << "Could not open PC2 device, error:" << result;
        throw eDeviceError;
    }
    else {
        result = libusb_claim_interface(this->pc2_handle, 0);

        if (result < 0) {
            BOOST_LOG_TRIVIAL(error) << "Could not claim libusb interface, error:" << result;
            throw eDeviceError;
        }
        BOOST_LOG_TRIVIAL(info) << "Opened PC2 device";
        this->transfer_in = libusb_alloc_transfer(0);
        this->transfer_out = libusb_alloc_transfer(0);
        // FIXME: Change read_callback to accept an instance pointer to avoid
        // this singleton mishegas
        libusb_fill_interrupt_transfer( 
                this->transfer_in, this->pc2_handle, USB_ENDPOINT_IN,
                this->input_buffer,  512,
                PC2DeviceIO::read_callback, NULL, 0); 
        libusb_submit_transfer(this->transfer_in);
    }
}

/** \brief Shut down event-handling thread, deregister singleton, and close device
 */
PC2DeviceIO::~PC2DeviceIO() {
    this->keep_running = false;
    libusb_close(this->pc2_handle);
    this->usb_thread->join();
    singleton = nullptr;
}

// FIXME: Type confusion here. Is PC2Message implicitly cast from a PC2Telegram?
void PC2DeviceIO::write(const PC2Message &message) {
    // TODO: Figure out syntax to create vector of proper size
    std::vector<uint8_t> telegram; //(message.size() + 2);
    // start of transmission
    telegram.push_back(0x60);
    // nbytes
    telegram.push_back(message.size());
    telegram.insert(telegram.end(), message.begin(), message.end());
    // end of transmission
    telegram.push_back(0x61);

    // if we're asked to send a telegram, print a debug representation of it
    if(telegram[2] == 0xe0) {
        MasterlinkTelegram foo = telegram;
        TelegramPrinter::print(foo);
    }

    // This mutex is unlocked in the callback
    if(!this->transfer_out_mutex.try_lock_for(std::chrono::seconds(1))) {
        BOOST_LOG_TRIVIAL(error) << "Timed out waiting for mutex while sending message in PC2DeviceIO::write!";
        // Try for one more second, crap out if it doesn't work
        assert(this->transfer_out_mutex.try_lock_for(std::chrono::seconds(1)));
    }; 

    // FIXME: USB_ENDPOINT_OUT is the wrong value. Why?
    libusb_fill_interrupt_transfer( 
            this->transfer_out, this->pc2_handle, 0x01, //USB_ENDPOINT_OUT,
            telegram.data(),  telegram.size(),
            PC2DeviceIO::write_callback, NULL, 0); 
    libusb_submit_transfer(this->transfer_out);

    std::string debug_message = "Sending:";
    for (auto &x : telegram)
        debug_message.append(boost::str(boost::format(" %02X") % (unsigned int)x));
    BOOST_LOG_TRIVIAL(debug) << debug_message;
}

void PC2DeviceIO::write_callback(struct libusb_transfer *transfer) {
    singleton->transfer_out_mutex.unlock();
    assert(transfer->status == LIBUSB_TRANSFER_COMPLETED);
    BOOST_LOG_TRIVIAL(debug) << "Write successful";
}

void PC2DeviceIO::read_callback(struct libusb_transfer *transfer) {
    // FIXME: Handle non-successful transfers more elegantly
    assert(transfer->status == LIBUSB_TRANSFER_COMPLETED);
    BOOST_LOG_TRIVIAL(debug) << "Read successful";
    assert(!libusb_submit_transfer(singleton->transfer_in));
    PC2Message msg(singleton->input_buffer, singleton->input_buffer + transfer->actual_length);
    std::string debug_message = "received:";
    for (auto &x : msg)
        debug_message.append(boost::str(boost::format(" %02X") % (unsigned int)x));
    BOOST_LOG_TRIVIAL(debug) << debug_message;
    uint8_t bogus_message[] = {0xFF, 0xFF, 0x01, 0xFF, 0xFF };
    if(transfer->actual_length == 5 && !memcmp(singleton->input_buffer, bogus_message, 5)) {
        BOOST_LOG_TRIVIAL(info) << "Got bogus message; ignoring...";
        return;
    }

    // in some cases, a 60 ... 61 message can occur _inside_ a multi-part message,
    // so FIXME: prepare for that eventuality!

    // Are we waiting for a message continuation?
    if(singleton->reassembly_buffer.size()) {
        BOOST_LOG_TRIVIAL(debug) << "Continuation mode, "<< singleton->remaining_bytes << " remaining...";
        // Yes, we're in continuation mode
        // - add the message to continuation buffer
        singleton->reassembly_buffer.insert(singleton->reassembly_buffer.end(),
                singleton->input_buffer, 
                singleton->input_buffer + transfer->actual_length);
        if(singleton->remaining_bytes <= transfer->actual_length) {
            singleton->remaining_bytes = 0;
        } else {
            singleton->remaining_bytes -= transfer->actual_length;
        }
        // was that the last of it?
        if(!singleton->remaining_bytes) {
            BOOST_LOG_TRIVIAL(debug) << "Continuation mode done";
            singleton->inbox.push(singleton->reassembly_buffer);
            singleton->reassembly_buffer.clear();
            std::unique_lock<std::mutex> lk(singleton->mutex);
            singleton->data_waiting.notify_one();
            lk.unlock();
        }
    } else {
        // The minimum valid message is 3 bytes; anything less
        // is a problem
        assert(transfer->actual_length >= 3);
        // is singleton message continued?
        int msg_length = (singleton->input_buffer[1] + 3);
        BOOST_LOG_TRIVIAL(debug) << "Message length: " << msg_length << " bytes";
        if(msg_length > transfer->actual_length) {
            // Yes, append to continuation buffer
            singleton->reassembly_buffer.insert(singleton->reassembly_buffer.end(),
                    singleton->input_buffer, 
                    singleton->input_buffer + transfer->actual_length);
            singleton->remaining_bytes = msg_length - transfer->actual_length;
            BOOST_LOG_TRIVIAL(debug) << "Continuation mode started, expecting " << singleton->remaining_bytes << " more bytes";
        } else {
            // Nope, it's just a simple message.
            PC2Message msg(singleton->input_buffer, singleton->input_buffer + transfer->actual_length);
            singleton->inbox.push(msg);
            std::unique_lock<std::mutex> lk(singleton->mutex);
            singleton->data_waiting.notify_one();
            lk.unlock();
        }
    }
}

PC2Telegram PC2DeviceIO::read() {
    BOOST_LOG_TRIVIAL(debug) << "Read waiting for semaphore...";

    std::unique_lock<std::mutex> lk(this->mutex);
    this->data_waiting.wait(lk);
    lk.unlock();

    auto msg = this->inbox.front();
    this->inbox.pop();

    std::string debug_message = " Got:";
    for (auto &x : msg) {
        debug_message.append(boost::str(boost::format(" %02X") % (unsigned int)x));
    }
    BOOST_LOG_TRIVIAL(debug) << debug_message;

    return msg;
}

void PC2Device::open() {
    this->usb_device.open();
    this->event_thread = new std::thread (&PC2Device::event_loop, this);
};

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

/**! \brief Loop indefinitely handling USB events
 */
void PC2DeviceIO::usb_loop() {
    while(this->keep_running) {
        libusb_handle_events(this->usb_ctx);
    }
}

void PC2Device::event_loop() {
    BOOST_LOG_TRIVIAL(info) << "PC2Device event thread running...";
    while(1) {
        PC2Telegram telegram;
        telegram = this->usb_device.read();
        BOOST_LOG_TRIVIAL(info) << "Got telegram; signalling semaphore";
        this->inbox.push(telegram);
        sem_post(&this->pc2->semaphore);
    }
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
