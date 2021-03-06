#include <future>
#include "masterlink/node.hpp"
#include "masterlink/telegram.hpp"
#include "pc2/pc2.hpp"

bool VideoMaster::is_present(PC2 * pc2) {
    DecodedTelegram::MasterPresent request;
    request.telegram_type = request.telegram_types::request;
    request.payload_version = 4;
    request.payload = {0x0a, 0x01, 0x00};
    request.dest_node = 0xc0;
    request.src_node = 0x01;
    try {
        pc2->beolink->interrogate(request);
    } catch (TelegramRequestTimeout e) {
        return false;
    }
    return true;
}
