/*! \file masterlink/protocol.hpp
 *  \brief Named Masterlink protocol values: node addresses, telegram and payload types, payload
 *         subtypes ("versions") and the layout of the payloads AudioMaster and SourceCenter send.
 *
 *  Where the names come from (each value below says which applies):
 *  - **B&O**: B&O's own names, from BeoIpcML.exe (BeoMaster 5 software 7.04). Its debug-name function
 *    (FUN_0421a1f4) names telegram/payload pairs; its log strings name some fields.
 *  - **decompiled**: the meaning follows from the code in BeoIpcML.exe that builds or parses that
 *    telegram (addresses given, e.g. FUN_0422c3b8 builds SOURCE_STATUS), not from a name.
 *  - **observed**: seen on the wire in USB captures or live Masterlink sniffs, meaning inferred.
 *  - **TODO**: the meaning is not known. The value is what B&O's software sends.
 *
 *  The telegram layout, as BeoIpcML builds it:
 *
 *      dest src 01 TYPE dest_src src_src 00 PAYLOAD_TYPE SIZE SUBTYPE payload[SIZE] checksum 00
 *
 *  SUBTYPE is what MasterlinkTelegram calls payload_version. It selects the meaning and format of
 *  the payload (e.g. LINK_CONNECT_STATUS subtype 5 = CONNECTED_SIGNAL, 7 = DISCONNECTED_SIGNAL), so
 *  each payload type has its own set of subtypes below.
 *
 *  See also reports/masterlink-protocol.md and reports/pc2-control.md in the rea-local workspace.
 */
#ifndef __ML_PROTOCOL_HPP
#define __ML_PROTOCOL_HPP

#include <cstdint>
#include <vector>

namespace ml {

    /*! Masterlink node addresses. Room nodes are 0x01-0x13. */
    enum class Node : uint8_t {
        all = 0x80,                 ///< B&O: (broadcast) "ALL"
        all_source_users = 0x83,    ///< B&O: ALL_SOURCE_USERS (every node that can listen to a source)
        video_master = 0xc0,        ///< V_MASTER
        audio_master = 0xc1,        ///< A_MASTER
        pc_1 = 0xc2,                ///< PC_1; the MLGW calls a BeoMaster 5 at this address SOURCE_CENTER
        source_center = pc_1,
    };

    constexpr uint8_t addr(Node n) { return static_cast<uint8_t>(n); }

    /*! Telegram types (byte 3). Same values as MasterlinkTelegram::telegram_types. */
    enum class TelegramType : uint8_t {
        command = 0x0a,
        request = 0x0b,
        status = 0x14,
        info = 0x2c,
    };

    /*! Payload types (byte 7), with B&O's names. Older libpc2 names in brackets. */
    enum class PayloadType : uint8_t {
        link_info = 0x04,           ///< B&O: LINK_INFO_COMMAND / LINK_INFO_STATUS [MASTER_PRESENT]
        audio_bus = 0x08,           ///< no B&O name; the audio bus arbitration question/answer [AUDIO_BUS]
        source_control = 0x0d,      ///< B&O: SOURCE_CONTROL (a Beo4 key for a source) [BEO4_KEY]
        standby_0 = 0x10,           ///< B&O: STANDBY_0 (stop / take the bus) [STANDBY]
        standby_1 = 0x11,           ///< B&O: STANDBY_1 (leave / all standby) [RELEASE]
        standby_2 = 0x12,           ///< B&O: STANDBY_2
        bus_released = 0x1c,        ///< no B&O name; observed: confirms the audio bus was handed over
        ir_simul_low = 0x20,        ///< B&O: IR_SIMUL_LOW (a simulated Beo4 key press on another node)
        local_source = 0x30,        ///< no B&O name; observed: what a master plays locally
        programming_status = 0x40,  ///< B&O: PROGRAMMING_STATUS (the clock broadcast) [CLOCK]
        link_connect_0 = 0x44,      ///< B&O: LINK_CONNECT_COMMAND_0 / LINK_CONNECT_STATUS [TRACK_INFO]
        link_connect_1 = 0x45,      ///< B&O: LINK_CONNECT_COMMAND_1 (a room asks for a source) [GOTO_SOURCE]
        link_connect_2 = 0x46,      ///< B&O: LINK_CONNECT_COMMAND_2
        sl_config = 0x48,           ///< B&O: SL_CONFIG_STATUS
        lock_manager = 0x5c,        ///< B&O: LOCK_MANAGER_COMMAND / LOCK_MANAGER_STATUS (the bus lock, "function key")
        source_connect = 0x6c,      ///< B&O: SOURCE_CONNECT_COMMAND / SOURCE_CONNECT_STATUS [DISTRIBUTION_REQUEST]
        pri_system_status = 0x82,   ///< B&O: PRI_SYSTEM_STATUS [TRACK_INFO_LONG]
        source_status = 0x87,       ///< B&O: SOURCE_STATUS [STATUS_INFO]
    };

    /*! Masterlink source IDs used here (the full list is Masterlink::source). */
    namespace source {
        constexpr uint8_t pc = 0x47;    ///< the BeoMaster 5's own PC source
    }

    /*! The audio and video links, as flag pairs in STANDBY_0, STANDBY_1 v2 and LINK_CONNECT payloads
     *  (decompiled: FUN_04229034, FUN_04229358, FUN_04228e2c). */
    namespace link {
        constexpr uint8_t off = 0x00;
        constexpr uint8_t on = 0x01;
    }

    // ------------------------------------------------------------------ LINK_INFO (0x04)
    /*! "Is there a master?" REQUEST and its STATUS answer.
     *  Decompiled: FUN_04223034 builds the request, FUN_042233a0 the answer. */
    namespace link_info {
        constexpr uint8_t subtype = 4;                  ///< request and answer
        constexpr uint8_t subtype_own_node_probe = 23;  ///< BeoIpcML's own node to V_MASTER, payload "01"; TODO

        /*! Payload byte 0: which kind of node is talking. In a request it is the requester's kind; in an
         *  answer it is the master the answer speaks for (01 if sent as C1, 02 if sent as C0). */
        enum class NodeKind : uint8_t {
            audio_master = 0x01,    ///< BeoIpcML SystemConfiguration 4, 5, 6
            video_master = 0x02,    ///< SystemConfiguration 2
            config_1 = 0x03,        ///< SystemConfiguration 1; TODO: name
            link_room_08 = 0x08,    ///< observed from room nodes; TODO: BeoIpcML never sends it
            link_room = 0x0a,       ///< SystemConfiguration 3, 7-10 (link rooms, source center)
        };
        /*! Payload byte 1. TODO: BeoIpcML sends 02 instead in SystemConfiguration 5 ("A.OPT = 0"), and
         *  the PC2 firmware answers 02 for a sleeping PC. Everything on the wire so far has 01. */
        constexpr uint8_t byte1_default = 0x01;
        /*! Payload byte 2: 00 in a request; in an answer, whether that master exists (BeoIpcML answers
         *  01 as C1 only if it is configured as audio master, and always 01 as C0). */
        constexpr uint8_t absent = 0x00;
        constexpr uint8_t present = 0x01;

        inline std::vector<uint8_t> request(NodeKind from) {
            return {(uint8_t)from, byte1_default, absent};
        }
        inline std::vector<uint8_t> answer(NodeKind as, bool is_present) {
            return {(uint8_t)as, byte1_default, is_present ? present : absent};
        }
    }

    // ------------------------------------------------------------------ AUDIO_BUS (0x08)
    /*! "Are you using the audio bus?" Decompiled: FUN_04224950 (request), FUN_04224e7c (answer). */
    namespace audio_bus {
        constexpr uint8_t subtype_request = 1;      ///< empty payload
        constexpr uint8_t subtype_free = 4;         ///< answer, empty payload: not distributing
        constexpr uint8_t subtype_busy = 6;         ///< answer: distributing, payload below
        /*! Busy payload: 02 01 <flag> <source>. TODO: the meaning of 02 and 01. <flag> is a BeoIpcML
         *  object field (always 00 in captures). A BeoSound 3000 appends a fifth byte, 01. */
        constexpr uint8_t busy_byte0 = 0x02;
        constexpr uint8_t busy_byte1 = 0x01;
        constexpr uint8_t busy_flag_default = 0x00;  ///< TODO

        inline std::vector<uint8_t> busy(uint8_t src) {
            return {busy_byte0, busy_byte1, busy_flag_default, src};
        }
    }

    // ------------------------------------------------------------------ 0x30 (local source)
    /*! Decompiled: FUN_04229e30 (request), FUN_042311d8 (answer). No B&O name. */
    namespace local_source {
        constexpr uint8_t subtype_request = 2;      ///< empty payload
        constexpr uint8_t subtype_none = 4;         ///< answer, empty payload: nothing plays locally
        constexpr uint8_t subtype_playing = 6;      ///< answer: payload 08 <source>
        constexpr uint8_t playing_marker = 0x08;    ///< TODO: meaning (constant in BeoIpcML)

        inline std::vector<uint8_t> playing(uint8_t src) { return {playing_marker, src}; }
    }

    // ------------------------------------------------------------------ LINK_CONNECT (0x44 / 0x45)
    /*! A room connecting to (or being moved off) a master's source.
     *  For 0x44 the subtype is B&O's "ConnectType" (log text: "prTGtrxLINK_CONNECT_STATUS: Invalid
     *  ConnectType", "RxD: STA:CONNECTED_SIGNAL", "Transmitting LINK_CONNECT_COMMAND / DISCONNECT_SIGNAL").
     *  Decompiled: FUN_04228e2c (0x45 request), FUN_04230ae0 (0x44 status), FUN_04230ec0 (0x44 request). */
    namespace link_connect {
        enum class Signal : uint8_t {
            connect = 1,            ///< LINK_CONNECT_COMMAND_1: "connect me to <source>"
            disconnect = 3,         ///< LINK_CONNECT_COMMAND_0, B&O: DISCONNECT_SIGNAL ("<cur> -> <wanted>, please")
            connected = 5,          ///< LINK_CONNECT_STATUS, B&O: CONNECTED_SIGNAL
            disconnected = 7,       ///< LINK_CONNECT_STATUS; inferred: B&O's DISCONNECTED_SIGNAL (source changed / given up)
            status_9 = 9,           ///< LINK_CONNECT_STATUS with a 16-bit value; TODO
        };

        /*! Payload byte 0 (decompiled: the 0x45 builder sets 02 together with the audio-link flag and 01
         *  together with the video-link flag). */
        enum class LinkType : uint8_t {
            video = 0x01,
            audio = 0x02,
        };
        constexpr uint8_t byte2 = 0x00;     ///< TODO: constant in every builder
        constexpr uint8_t byte3 = 0x02;     ///< TODO: constant in every builder

        /*! Payload byte 6. TODO: in DISCONNECT_SIGNAL B&O uses 02 = plain (8 bytes) and 03 = with the
         *  wanted source appended (13 bytes); in statuses it is a parameter (00 when connected, 02 in the
         *  change notice to listeners, 03 in the answer to a DISCONNECT_SIGNAL). */
        enum class Mode : uint8_t {
            none = 0x00,
            changed = 0x02,
            with_wanted_source = 0x03,
        };
        /*! Bytes 7 and 11 of the 13-byte "with wanted source" form: 01 ... <byte 11> <wanted>.
         *  TODO: B&O's builder writes 01 and 02; a BeoSound 3000 sent ff and 03 in its place. */
        constexpr uint8_t wanted_form_byte7 = 0x01;
        constexpr uint8_t wanted_byte11_video_master = 0x02;   ///< as a VM sent it (and BeoIpcML)
        constexpr uint8_t wanted_byte11_audio_master = 0xff;   ///< as a BeoSound 3000 sent it
        constexpr uint8_t wanted_byte11_answer = 0x03;         ///< in the answer to the request

        /*! 02 <src> 00 02 <audio> <video> */
        inline std::vector<uint8_t> head(uint8_t src) {
            return {(uint8_t)LinkType::audio, src, byte2, byte3, link::on, link::off};
        }
        /*! LINK_CONNECT_COMMAND_1 (subtype connect): ask a master for its source, over the audio link. */
        inline std::vector<uint8_t> connect_request(uint8_t src) {
            auto p = head(src);
            p.push_back(0x00);      // TODO (decompiled: always 00)
            return p;
        }
        /*! LINK_CONNECT_STATUS (subtype connected), 8 bytes. */
        inline std::vector<uint8_t> connected(uint8_t src) {
            auto p = head(src);
            p.insert(p.end(), {(uint8_t)Mode::none, 0x00});
            return p;
        }
        /*! The 13-byte form: head, <mode>, then 01 00 00 00 / 00 00 00 00, <byte 11>, <second source>. */
        inline std::vector<uint8_t> with_second_source(uint8_t src, Mode mode, bool wanted_form,
                                                       uint8_t byte11, uint8_t second) {
            auto p = head(src);
            p.insert(p.end(), {(uint8_t)mode, wanted_form ? wanted_form_byte7 : (uint8_t)0x00, 0x00, 0x00, 0x00,
                               byte11, second});
            return p;
        }
        /*! LINK_CONNECT_STATUS (subtype disconnected) to a listener: "<old> is gone, now <new>". */
        inline std::vector<uint8_t> changed(uint8_t old_src, uint8_t new_src) {
            return with_second_source(old_src, Mode::changed, false, 0x00, new_src);
        }
    }

    // ------------------------------------------------------------------ SOURCE_STATUS (0x87)
    /*! Decompiled: code at 0x422c3b8 builds all forms; B&O names subtypes 1-4 (FUN_0421a1f4). */
    namespace source_status {
        enum class Subtype : uint8_t {
            static_status = 1,      ///< B&O: STATIC_SOURCE_STATUS
            dynamic_status = 2,     ///< B&O: DYNAMIC_SOURCE_STATUS
            ascii_status = 3,       ///< B&O: ASCII_SOURCE_STATUS
            system_status = 4,      ///< B&O: SOURCE_SYSTEM_STATUS (the one we send)
        };

        /*! Source state, payload byte 11. Inferred: the field is the same BeoIpcML object field as
         *  PRI_SYSTEM_STATUS byte 3, and its three observed values match the three forms. */
        enum class State : uint8_t {
            idle = 0x00,
            playing = 0x02,
            stopped = 0x06,         ///< the "everything switched off" form
        };

        /*! Bytes 2-5: two 16-bit masks. The second is 0x1fbe, built by OR-ing bits 1-5, 7-12 one by one;
         *  the first is a function of it that is 0000 in every capture. TODO: what the bits stand for. */
        constexpr uint8_t mask_a_hi = 0x00, mask_a_lo = 0x00;
        constexpr uint8_t mask_b_hi = 0x1f, mask_b_lo = 0xbe;

        /*! SOURCE_SYSTEM_STATUS for `src` in state `state` (idle or playing): 31 bytes.
         *  The TODO bytes are constants or caller parameters in BeoIpcML; the values are what it sends. */
        inline std::vector<uint8_t> system_status(uint8_t src, State state) {
            return {src, 0x01,                      // 0x01: TODO (constant)
                    mask_a_hi, mask_a_lo, mask_b_hi, mask_b_lo,
                    0x01,                           // TODO (constant)
                    0x00, 0x00,                     // 16-bit field (BeoIpcML +0x50; also in PRI_SYSTEM)
                    0x00,                           // field +0x4d (also PRI_SYSTEM byte 2); TODO
                    0xff,                           // TODO (constant)
                    (uint8_t)state,
                    0x01,                           // 1 = audio source, 2 = video (for PC: 1-3); TODO
                    0x00,                           // caller parameter; TODO
                    0x03,                           // TODO (constant)
                    0x01,                           // caller parameter; TODO
                    0x01, 0x01, 0x03, 0x00, 0x02,   // TODO (constants)
                    0x00, 0x00, 0x00,
                    0x00,                           // caller parameter (same as byte 13); TODO
                    0x01,                           // caller parameter; TODO
                    0x00, 0x00, 0x00, 0x00,
                    0x00};                          // caller parameter; TODO
        }
        /*! The "stopped" form (26 bytes): the fields are ff and the state is stopped. */
        inline std::vector<uint8_t> stopped(uint8_t src) {
            return {src, 0x01, mask_a_hi, mask_a_lo, mask_b_hi, mask_b_lo, 0x01,
                    0xff, 0xff, 0xff, 0xff,
                    (uint8_t)State::stopped,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0x00, 0x00, 0x00,
                    0x00,                           // caller parameter (as byte 13 above); TODO
                    0x01};                          // caller parameter (as byte 25 above); TODO
        }
    }

    // ------------------------------------------------------------------ PRI_SYSTEM_STATUS (0x82)
    /*! Decompiled: code at 0x422c2fc. Payload: 06 <src> <field 4d> <state> 00 00 00 <16-bit field 50> 01,
     *  where <field 4d>, <state> and the 16-bit field are the same values as in SOURCE_STATUS. */
    namespace pri_system {
        constexpr uint8_t subtype_audio = 1;    ///< sent for an audio source (BeoIpcML: field 45 set)
        constexpr uint8_t subtype_video = 2;    ///< otherwise; observed from a video master
        constexpr uint8_t byte0 = 0x06;         ///< TODO: constant in BeoIpcML
        constexpr uint8_t last_byte = 0x01;     ///< TODO: constant in BeoIpcML

        /*! For a source that is playing (the only form we send). */
        inline std::vector<uint8_t> playing(uint8_t src) {
            return {byte0, src, 0x00, (uint8_t)source_status::State::playing, 0x00, 0x00, 0x00,
                    0x00, 0x00, last_byte};
        }
    }

    // ------------------------------------------------------------------ PROGRAMMING_STATUS (0x40)
    /*! The clock broadcast. Decompiled: FUN_042257a0. Payload (BCD):
     *  0a 00 03 hh mm ss 00 DD MM YY <ClockPriority>. */
    namespace clock {
        constexpr uint8_t subtype = 11;
        constexpr uint8_t byte0 = 0x0a;     ///< TODO: constant in BeoIpcML
        constexpr uint8_t byte1 = 0x00;     ///< TODO: constant
        constexpr uint8_t byte2 = 0x03;     ///< TODO: constant
        constexpr uint8_t byte6 = 0x00;     ///< TODO: constant
        /*! Last byte: the "ClockPriority" registry value (HKCU\Software\Bang & Olufsen\Beolink PC).
         *  02 in every capture. */
        constexpr uint8_t default_priority = 0x02;
    }

    // ------------------------------------------------------------------ STANDBY_0 / STANDBY_1 (0x10 / 0x11)
    /*! Decompiled: FUN_04229034 (STANDBY_0), FUN_04225594 (STANDBY_1 to ALL), FUN_04229358 (STANDBY_1 v2). */
    namespace standby {
        constexpr uint8_t subtype_0 = 3;            ///< STANDBY_0: <audio link> <video link> <reason>
        constexpr uint8_t subtype_1_all = 1;        ///< STANDBY_1 to ALL, empty: "all standby"
        constexpr uint8_t subtype_1_node = 2;       ///< STANDBY_1: <audio link> <video link>, one node leaves

        /*! STANDBY_0 byte 2. B&O sends 1 normally; when it closes a link because of the device type it
         *  sends 2 for an audio device and 3 for a video device. */
        enum class Reason : uint8_t {
            standby = 0x01,             ///< TODO: name. Observed when a master takes the bus
            close_audio_link = 0x02,    ///< observed from listeners after a DISCONNECTED_SIGNAL
            close_video_link = 0x03,
        };

        /*! STANDBY_0 on the audio link: a master takes the audio bus from the other master. */
        inline std::vector<uint8_t> take_audio_bus() {
            return {link::on, link::off, (uint8_t)Reason::standby};
        }
        /*! STANDBY_1 subtype 2 on the audio link: we leave. */
        inline std::vector<uint8_t> leave_audio() { return {link::on, link::off}; }
    }

    // ------------------------------------------------------------------ 0x1C (bus released)
    /*! No B&O name. COMMAND payload: 01 [nodes...]. Decompiled: FUN_04225010 sends subtype 1 with
     *  01 followed by a list of nodes (a video master sent "01 04" while node 04 was listening).
     *  A BeoSound 3000 sent subtype 2 with just "01", which is what we copy. */
    namespace bus_released {
        constexpr uint8_t subtype_with_nodes = 1;
        constexpr uint8_t subtype_bs3000 = 2;
        constexpr uint8_t byte0 = 0x01;     ///< TODO: constant
    }

    // ------------------------------------------------------------------ LOCK_MANAGER (0x5C)
    /*! The bus lock ("function key"). Decompiled: FUN_04223f74 (REQUEST/COMMAND), FUN_04223fe8 (STATUS);
     *  the PC2 firmware runs the same exchange (pc2-firmware.md). Payload is always "01". */
    namespace lock_manager {
        constexpr uint8_t subtype_request = 1;          ///< REQUEST to ALL: who holds the lock?
        constexpr uint8_t subtype_holder = 2;           ///< STATUS from the holder
        constexpr uint8_t subtype_holder_alt = 3;       ///< STATUS, other form; TODO
        constexpr uint8_t subtype_take = 4;             ///< COMMAND to the holder: hand it over
        constexpr uint8_t subtype_command_alt = 5;      ///< COMMAND, other form; TODO
        constexpr uint8_t payload = 0x01;
    }

    // ------------------------------------------------------------------ SOURCE_CONNECT (0x6C)
    /*! Decompiled: FUN_042338d8 builds the STATUS (subtype 8) with a one- or two-byte payload. */
    namespace source_connect {
        constexpr uint8_t subtype_status = 8;
        constexpr uint8_t connected = 0x01;     ///< inferred from the flow; always 01 in captures
    }

    // ------------------------------------------------------------------ IR_SIMUL_LOW (0x20)
    /*! A Beo4 key pressed on another node: 00 01 ff ff <key>. Observed (as libpc2's send_beo4_code()). */
    namespace ir_simul {
        constexpr uint8_t subtype = 2;
        inline std::vector<uint8_t> key(uint8_t beo4_key) {
            return {0x00, 0x01, 0xff, 0xff, beo4_key};  // TODO: the first four bytes
        }
    }

    // ------------------------------------------------------------------ SOURCE_CONTROL (0x0D)
    /*! COMMAND subtype 1, payload <source> <Beo4 key>. Decompiled: FUN_04228f48. */
    namespace source_control {
        constexpr uint8_t subtype = 1;
    }
}

#endif
