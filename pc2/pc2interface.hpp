#ifndef __PC2INTERFACE_HPP
#define __PC2INTERFACE_HPP

class PC2Interface; 

#include "ml/telegram.hpp"
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
    virtual void beo4_press(uint8_t keycode) = 0;
    Beolink::LocalSourceList list;
};

#endif
