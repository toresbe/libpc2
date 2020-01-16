#ifndef __ML_MASTERLINK_HPP
#define __ML_MASTERLINK_HPP
#include <string>
#include <map>
class Masterlink {
    public:
    enum source {
        tv = 0x0b,
        v_mem = 0x15,
        v_tape = 0x15,
        v_tape2 = 0x16,
        dvd2 = 0x16,
        dtv = 0x1f,
        sat = 0x1f,
        dvd = 0x29,
        v_aux = 0x33,
        dtv2 = 0x33,
        v_aux2 = 0x3e,
        doorcam = 0x3e,
        pc = 0x47,
        a_mem = 0x79,
        a_tape = 0x79,
        a_mem2 = 0x7a,
        cd = 0x8d,
        a_aux = 0x97, 
        n_radio = 0xa1,
        phono = 0xa1,
        radio = 0x6f,
    };

    std::map<Masterlink::source, std::string> source_name = {
        {Masterlink::source::tv, "TV"},
        {Masterlink::source::v_mem, "V_MEM"},
        {Masterlink::source::v_tape, "V_TAPE"},
        {Masterlink::source::pc, "PC"},
        {Masterlink::source::dvd2, "DVD2"},
        {Masterlink::source::a_mem2, "A.MEM2"},
        {Masterlink::source::doorcam, "DOORCAM"},
        {Masterlink::source::v_aux2, "V.AUX2"},
        {Masterlink::source::v_tape2, "V.TAPE2"},
        {Masterlink::source::cd, "CD"},
        {Masterlink::source::sat, "SAT"},
        {Masterlink::source::dtv, "DTV"},
    };
};
#endif
