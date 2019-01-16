// Masterlink Telegram pretty-printer

#include <string>
#include <iostream>
#include "telegram.cpp"
#include <boost/format.hpp>

// TODO: convert to iostreams?

const char * header_fmt = "\x1b[1;37;44m%|=20s||%|=20s||%|=20s||%|=20s||%|=20s||%|=20s| \x1b[0m";
const char * row_fmt = "\x1b[1;37;44m%|=20s||%|=20s||%|=20s||%|=20s||%|14d| bytes|%|=20d| \x1b[0m";
class TelegramPrinter {
    public:
        static void print_heading() {
            std::cout << ((boost::format(header_fmt) % "src" % "dest" % "tgram type" % "payload type" % "payload size" % "payload version")).str() + "\n";
        }

        static void print_header(MasterlinkTelegram & tgram) {
            auto formatted = boost::format(row_fmt) \
                             % tgram.node_name[tgram.src_node] \
			     % tgram.node_name[tgram.dest_node] \
                             % tgram.telegram_type_name[tgram.telegram_type] \
                             % tgram.payload_type_name[tgram.payload_type] \
                             % tgram.payload_size \
                             % tgram.payload_version;
            std::cout << formatted.str() << "\n";
        }


        static void print_metadata(MasterlinkTelegram & tgram) {
        }

        static void print(MasterlinkTelegram & tgram) {
            print_heading();
            print_header(tgram);
            DecodedMessage * m = DecodedMessageFactory::make(tgram);
            std::cout << *m << std::endl;
        };
    private:
        TelegramPrinter() {};
};
