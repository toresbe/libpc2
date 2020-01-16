#include "fragment_assembler.hpp"
#include <boost/log/trivial.hpp>
#include <iostream>
#include <chrono>
#include <boost/format.hpp>

class PC2InvalidMessage : public std::runtime_error {
public:
  PC2InvalidMessage(const std::string& message) : runtime_error(message) {

  }
  int errorCode;
};

void PC2MessageFragmentAssembler::operator<< (const PC2Message &fragment) {
    // in some cases, a 60 ... 61 message can occur _inside_ a multi-part message,
    // so FIXME: prepare for that eventuality!
    // Do not add fragments if there's a message waiting
    assert(!assembled_message.size());
    // Are we waiting for a message continuation?
    if(reassembly_buffer.size()) {
        BOOST_LOG_TRIVIAL(debug) << "Continuation mode, "<< remaining_bytes << " remaining...";
        // Yes, we're in continuation mode
        // - add the message to continuation buffer
        reassembly_buffer.insert(reassembly_buffer.end(), fragment.begin(), fragment.end());
        if(remaining_bytes <= fragment.size()) {
            remaining_bytes = 0;
        } else {
            remaining_bytes -= fragment.size();
        }
        // was that the last of it?
        if(!remaining_bytes) {
            BOOST_LOG_TRIVIAL(debug) << "Continuation mode done";
            assembled_message = reassembly_buffer;
            if(assembled_message[assembled_message.size()-1] != 0x61) {
                assembled_message.clear();
                throw PC2InvalidMessage("Assembled message does not end with 0x61!");
            };
        }
    } else {
        // The minimum valid message is 3 bytes; anything less is a problem
        if(fragment.size() < 3) {
            assembled_message.clear();
            throw PC2InvalidMessage("Received message too short to be valid!");
        };
        if(fragment[0] != 0x60) {
            assembled_message.clear();
            throw PC2InvalidMessage("Message fragment does not start with 0x60!");
        };

        // is singleton message continued?
        int msg_length = (fragment[1] + 3);
        BOOST_LOG_TRIVIAL(debug) << "Message length: " << msg_length << " bytes";
        if(msg_length > fragment.size()) {
            // Yes, append to continuation buffer
            reassembly_buffer.insert(reassembly_buffer.end(), fragment.begin(), fragment.end());
            remaining_bytes = msg_length - fragment.size();
            BOOST_LOG_TRIVIAL(debug) << "Continuation mode started, expecting " << remaining_bytes << " more bytes";
        } else {
            // Nope, it's just a simple message.
            assembled_message = fragment;
            if(assembled_message[assembled_message.size()-1] != 0x61) {
                assembled_message.clear();
                throw PC2InvalidMessage("Assembled message does not end with 0x61!");
            };
        }
    }
}

bool PC2MessageFragmentAssembler::has_complete_message() {
    return assembled_message.size();
}

PC2Message PC2MessageFragmentAssembler::get_message() {
    PC2Message returned_message = assembled_message;
    assembled_message.clear();
    return returned_message;
}
