// Masterlink Telegram pretty-printer

#include <string>
#include <iostream>
#include "pprint.hpp"
#include "telegram.hpp"
#include <boost/format.hpp>

// TODO: convert to iostreams?

void TelegramPrinter::print_heading() {
    std::cout << ((boost::format(header_fmt) % "src" % "dest" % "tgram type" % "payload type" % "payload size" % "payload version")).str() + "\n";
}

void TelegramPrinter::print_header(MasterlinkTelegram & tgram) {
    auto formatted = boost::format(row_fmt) \
                     % tgram.node_name[tgram.src_node] \
                     % tgram.node_name[tgram.dest_node] \
                     % tgram.telegram_type_name[tgram.telegram_type] \
                     % tgram.payload_type_name[tgram.payload_type] \
                     % tgram.payload_size \
                     % tgram.payload_version;
    std::cout << formatted.str() << "\n";
}


void TelegramPrinter::print(MasterlinkTelegram & tgram) {
    print_heading();
    print_header(tgram);
    DecodedMessage * m = DecodedMessageFactory::make(tgram);
    std::cout << *m << std::endl;
};
