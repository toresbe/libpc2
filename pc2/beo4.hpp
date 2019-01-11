#ifndef __BEO4_HPP
#define __BEO4_HPP

#define BEO4_KEY_TV 0x80
#define BEO4_KEY_PC 0x8B
#define BEO4_KEY_CD 0x92
#define BEO4_KEY_TV 0x80
#define BEO4_KEY_PHONO 0x93
#define BEO4_KEY_A_AUX 0x83
#include <cstdint>
#include <map>
#include <vector>
#include "pc2/pc2.hpp"

class Beo4 {
    public:
        static uint8_t source_from_keycode(uint8_t beo4_code) {
            std::map<uint8_t, uint8_t> map;
            map[BEO4_KEY_TV] = 0x0B;
            map[0x81] = 0x6F;
            map[0x82] = 0x33;
            map[0x85] = 0x16;
            map[0x86] = 0x29;
            map[0x8A] = 0x1F;
            map[BEO4_KEY_PC] = ML_SRC_PC; // PC
            map[0x8D] = 0x3E;
            map[0x91] = 0x79;
            map[0x92] = 0x8D;
            map[BEO4_KEY_PHONO] = ML_SRC_PHONO;
            return map[beo4_code];
        }

        static bool is_source_key(uint8_t beo4_code) {
            // TODO: Change these absolute hex values to defines or constants
            // 0x0C (stby) is not included here
            std::vector<uint8_t> source_keys = { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,\
                0x88, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x90, 0x91,\
                    0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0xA8, 0x47,\
                    0xFA };
            for (auto i : source_keys) {
                if (beo4_code == i) return true;
            }

            return false;
        }
};
#endif
