#ifndef __BEO4_HPP
#define __BEO4_HPP

class Beo4;

#include <cstdint>
#include <algorithm>
#include <map>
#include <vector>
#include "masterlink/masterlink.hpp"

class Beo4 {
    public:
        enum keycode {
            a_aux = 0x83,
            arrow_down = 0x1f,
            arrow_left = 0x32,
            arrow_right = 0x34,
            arrow_up = 0x1e,
            a_tape = 0x91,
            a_tape2 = 0x94,
            av = 0xbf,
            blue = 0xd8,
            cd = 0x92,
            digit_0 = 0x00,
            digit_1 = 0x01,
            digit_2 = 0x02,
            digit_3 = 0x03,
            digit_4 = 0x04,
            digit_5 = 0x05,
            digit_6 = 0x06,
            digit_7 = 0x07,
            digit_8 = 0x08,
            digit_9 = 0x09,
            go = 0x35,
            green = 0xd5,
            menu = 0x5c,
            mute = 0x0d,
            pc = 0x8b,
            phono = 0x93,
            picture = 0x45,
            play = 0x35,
            radio = 0x81,
            radio2 = 0x83,
            record = 0x37,
            red = 0xd9,
            sat = 0x8a,
            sat2 = 0x86,
            sound = 0x44,
            standby = 0x0c,
            stop = 0x36,
            store = 0x0b,
            text = 0x88,
            tv = 0x80,
            vol_down = 0x64,
            vol_up = 0x60,
            v_aux = 0x82,
            v_tape = 0x85,
            yellow = 0xd4,
        };

        std::map<keycode, Masterlink::source> source_from_keycode = {
            {keycode::tv, Masterlink::source::tv},
            {keycode::radio, Masterlink::source::radio},
            {keycode::v_aux, Masterlink::source::v_aux},
            {keycode::v_tape, Masterlink::source::v_tape},
            {keycode::sat2, Masterlink::source::dvd},
            {keycode::sat, Masterlink::source::sat},
            {keycode::pc, Masterlink::source::pc},
            //{keycode::v_aux2, Masterlink::source::v_aux2},
            {keycode::a_tape, Masterlink::source::a_tape},
            {keycode::cd, Masterlink::source::cd},
            {keycode::phono, Masterlink::source::phono},
        };

        static bool is_source_key(uint8_t beo4_code) {
            // TODO: Change these absolute hex values to defines or constants
            // 0x0C (stby) is not included here
            std::vector<uint8_t> source_keys = { 
                keycode::tv, keycode::radio, keycode::v_aux, keycode::a_aux, \
                0x84, 0x85, keycode::sat2, 0x87, \
                0x88, keycode::sat, keycode::pc, 0x8C, \
                0x8D, 0x8E, 0x90, keycode::a_tape,\
                keycode::cd, keycode::phono, keycode::a_tape2, 0x95, \
                0x96, 0x97, 0xA8, 0x47,\
                0xFA };

            if(std::find(source_keys.begin(), source_keys.end(), beo4_code) != source_keys.end())
                return true;

            return false;
        }
};
#endif
