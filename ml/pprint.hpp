#include "telegram.hpp"
#ifndef __ML_PPRINT_HPP
#define __ML_PPRINT_HPP

class TelegramPrinter {
    public:
        static void print_heading();
        static void print_header(MasterlinkTelegram & tgram);
        static void print_metadata(MasterlinkTelegram & tgram);
        static void print(MasterlinkTelegram & tgram);
    private:
        static const constexpr char * header_fmt = "\x1b[1;37;44m%|=10s||%|=10s||%|=16s||%|=20s||%|=10s||%|=10s| \x1b[0m";
        static const constexpr char * row_fmt = "\x1b[1;37;44m%|=10s||%|=10s||%|=16s||%|=20s||%|4d| bytes|%|=10d| \x1b[0m";
        TelegramPrinter() {};
};
#endif
