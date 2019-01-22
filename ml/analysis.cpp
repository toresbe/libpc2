#include <boost/format.hpp>
#include <iostream>
#include <string>
#include "ml/telegram.hpp"

std::string payload_header(std::string input) {
    std::string format_string = "\x1b[48;5;99m\x1b[97m\t%|=79s|\x1b[0m\n";
    return boost::str(boost::format(format_string) % "Payload");
};

std::string payload_format(int byte, std::string input) {
    std::string format_string = "\x1b[48;5;99m\x1b[97m   [%|02X|] \x1b[0m%|-74s|\n";
    return boost::str(boost::format(format_string) % byte % input);
};

std::ostream& MasterlinkTelegram::debug_repr(std::ostream& outputStream) {
    std::string analysis;
    analysis += payload_header("");
    int i = 0;
    for(auto x: this->payload) {
        analysis += payload_format(i++, boost::str(boost::format("%|02X|") % \
                    (unsigned int) x));
    }
    outputStream << analysis;
    return outputStream;
};

std::ostream& GotoSourceTelegram::debug_repr(std::ostream& outputStream) {
    std::string analysis; 
    analysis += payload_header("");
    int i = 0;
    for(auto x: this->payload) {
        analysis += payload_format(i++, boost::str(boost::format("%|02X|") % \
                    (unsigned int) x));
    }
    outputStream << analysis;
    return outputStream;
};

// new dump format
// {num_bytes, data_type, expected_value_policy, comment}
// expected_value_policy = std::pair<policy enum, std::vector<uint8_t, n>>
