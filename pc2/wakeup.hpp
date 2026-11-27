#ifndef __PC2_WAKEUP_HPP
#define __PC2_WAKEUP_HPP

#include <cstdint>
#include <vector>

typedef std::vector<uint8_t> PC2Message;

/*! \brief Raw 4-byte packets the PC2 sends around USB suspend, without "60 ... 61" framing.
 *
 *  From the PC2 firmware (BLMedia.hex v0236, see reports/pc2-control.md §9):
 *   - FF FF FF FF: the PC2 woke the PC (USB remote wakeup) because of a remote key that its
 *                  option allows to wake it (A.OPT rules / CompatabilityEnabled);
 *   - FF FC FE FF: the USB bus resumed (the PC woke up by itself).
 */
enum class PC2Wakeup { none, by_remote, bus_resumed };

inline PC2Wakeup pc2_wakeup_packet(const PC2Message &fragment) {
    if (fragment == PC2Message{0xff, 0xff, 0xff, 0xff}) return PC2Wakeup::by_remote;
    if (fragment == PC2Message{0xff, 0xfc, 0xfe, 0xff}) return PC2Wakeup::bus_resumed;
    return PC2Wakeup::none;
}

#endif
