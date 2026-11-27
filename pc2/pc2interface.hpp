#ifndef __PC2INTERFACE_HPP
#define __PC2INTERFACE_HPP

class PC2Interface; 

#include "masterlink/telegram.hpp"
#include "pc2/pc2.hpp"
#include "pc2/beo4.hpp"

class PC2Interface {
    public:
    PC2 * pc2;
    enum class address_mask_t {
        audio_master,
        beoport,
        promisc,
        source_center   ///< provide sources for another audio master (as a BeoMaster 5 does)
    };
    address_mask_t address_mask;
    /*! Our Masterlink node address, sent to the PC2 as "80 <address> 00" at start-up. 0 = the mode's
     *  default (audio master 0x01, Beoport 0x40, source center 0x29 = B&O's LastLegalAdr default).
     *  Updated from the PC2's ML-status replies (byte 5), as BeoIpcML does. */
    uint8_t ml_node_address = 0;
    virtual void beo4_press(Beo4::keycode keycode) = 0;

    // Masterlink hooks, used when address_mask is audio_master or source_center.
    /*! A link room wants source `src`: start it and return true, or false if unavailable. */
    virtual bool ml_start_source(uint8_t src) { (void)src; return false; }
    /*! No link room listens to our source any more. */
    virtual void ml_stop_source() {}
    /*! A Beo4 key from a link room aimed at our source (e.g. next track). */
    virtual void ml_source_key(uint8_t src, uint8_t key) { (void)src; (void)key; }
    /*! "All standby" was sent on the link. */
    virtual void ml_link_standby() {}

    /*! The PC2 reports a USB wake-up: by_remote = it woke the PC because of a remote key (raw
     *  packet FF FF FF FF), otherwise the bus resumed by itself (FF FC FE FF). */
    virtual void pc2_wakeup(bool by_remote) { (void)by_remote; }
    Beolink::LocalSourceList list;
};

#endif
