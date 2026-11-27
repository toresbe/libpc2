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
        promisc
    };
    address_mask_t address_mask;
    virtual void beo4_press(Beo4::keycode keycode) = 0;

    // Masterlink audio master hooks (used when address_mask is audio_master).
    /*! A link room wants source `src`: start it and return true, or false if unavailable. */
    virtual bool ml_start_source(uint8_t src) { (void)src; return false; }
    /*! No link room listens to our source any more. */
    virtual void ml_stop_source() {}
    /*! A Beo4 key from a link room aimed at our source (e.g. next track). */
    virtual void ml_source_key(uint8_t src, uint8_t key) { (void)src; (void)key; }
    /*! "All standby" was sent on the link. */
    virtual void ml_link_standby() {}
    Beolink::LocalSourceList list;
};

#endif
