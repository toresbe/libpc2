/*! \file masterlink/source_center.hpp
 *  \brief Masterlink "source center" behaviour: a node that only provides sources (address PC_1, 0xC2).
 *
 *  This is how a BeoMaster 5 behaves when another product is the audio master (observed live on a
 *  system with a BeoSound 3000 as audio master and a Beosystem 3 as video master; see
 *  reports/masterlink-protocol.md §6-§8). It never arbitrates for the bus. The audio master orders it:
 *    REQUEST SOURCE_CONNECT (0x6C), dest_src = source   -> start distributing that source
 *    COMMAND STANDBY_0 (0x10),     dest_src = source   -> stop it
 *    COMMAND STANDBY_1 (0x11),     dest_src = source   -> release it
 *    COMMAND SOURCE_CONTROL (0x0D), dest_src = source  -> a Beo4 key for it
 *  Its own panel does not request sources over the link; it sends the room's master the Beo4 key a
 *  remote would (COMMAND 0x20, IR_SIMUL_LOW), see send_remote_key().
 *
 *  Like AudioMaster, it holds no hardware or thread state: telegrams in through handle(), time through
 *  tick(), effects out through AudioMasterHost (the same host interface).
 */
#ifndef __ML_SOURCE_CENTER_HPP
#define __ML_SOURCE_CENTER_HPP

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

#include "masterlink/audio_master.hpp"

class SourceCenter {
    public:
        typedef std::chrono::steady_clock clock;

        static constexpr uint8_t addr_source_center = 0xc2;    ///< PC_1 / "SOURCE_CENTER"
        static constexpr uint8_t pt_source_connect = 0x6c;     ///< B&O: SOURCE_CONNECT_COMMAND / _STATUS
        static constexpr uint8_t pt_ir_simul = 0x20;           ///< B&O: IR_SIMUL_LOW (simulated Beo4 key)
        static constexpr uint8_t src_pc = 0x47;

        explicit SourceCenter(AudioMasterHost &host);

        /*! Startup: broadcast our idle SOURCE_STATUS. */
        void start(clock::time_point now = clock::now());
        void handle(MasterlinkTelegram &tgram, clock::time_point now = clock::now());
        /*! Call periodically: the delayed idle status after a source is stopped. */
        void tick(clock::time_point now = clock::now());

        /*! Press a Beo4 key on another node, as the BM5 panel does towards the room's master
         *  (e.g. dest = V_MASTER, key 0x94 "N.MUSIC", 0x80 "TV", 0x64 volume down, 0x0c standby).
         *  Observed telegram: dest C1 01 0A 00 47 00 20 05 02 | 00 01 ff ff <key>, sent from the
         *  audio master's address C1 (as captured; see report §7). */
        void send_remote_key(uint8_t dest, uint8_t key, uint8_t from = 0xc1);

        /*! Something plays locally (e.g. picked on the panel). While set, stopping a link source does
         *  not announce the idle status: observed when the panel's N.RADIO was still active (§8), vs
         *  ~2 s later when nothing played locally (§6). Inferred rule. */
        void set_local_playback(bool on);
        uint8_t current_source();
        bool distributing();

        static constexpr std::chrono::milliseconds idle_status_delay{2000};  ///< observed ~2 s

    private:
        MasterlinkTelegram make(uint8_t dest, uint8_t type, uint8_t ptype, uint8_t subtype,
                                std::vector<uint8_t> payload, uint8_t src_src = 0, uint8_t dest_src = 0,
                                uint8_t from = addr_source_center);
        void send(MasterlinkTelegram tgram);
        void send_status_active(uint8_t src);
        void send_status_stopped(uint8_t src);
        void send_status_idle();
        enum class Announce { nothing, idle_later, stopped };
        void stop(Announce announce, clock::time_point now);

        AudioMasterHost &host;
        std::recursive_mutex mutex;
        uint8_t source = 0;
        bool local_playback = false;
        bool idle_status_pending = false;
        clock::time_point idle_status_due;
};

#endif
