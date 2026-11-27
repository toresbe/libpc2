/*! \file masterlink/audio_master.hpp
 *  \brief Masterlink audio master behaviour: bus arbitration, source distribution, listeners.
 *
 *  Modelled on USB captures of B&O's own BeoSound 5 software acting as audio master (see
 *  reports/masterlink-protocol.md in the rea-local workspace). Names in comments follow B&O's
 *  own telegram names recovered from BeoIpcML.exe (LINK_INFO, LINK_CONNECT, SOURCE_STATUS, ...).
 *
 *  This class holds no hardware or thread state: telegrams come in through handle(), time
 *  through tick(), and everything it does goes out through AudioMasterHost. That keeps it
 *  testable by replaying captured conversations.
 */
#ifndef __ML_AUDIO_MASTER_HPP
#define __ML_AUDIO_MASTER_HPP

#include <chrono>
#include <functional>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <set>
#include <vector>

#include "masterlink/telegram.hpp"

/*! What the audio master needs from its surroundings. */
class AudioMasterHost {
    public:
        virtual ~AudioMasterHost() = default;
        /*! Send a Masterlink telegram (it will be serialized; checksum is computed). */
        virtual void send_telegram(MasterlinkTelegram &tgram) = 0;
        /*! Distribute our audio on the Masterlink audio bus (PC2 mixer: ml_distribute). */
        virtual void set_ml_distribution(bool on) = 0;
        /*! Play audio arriving over Masterlink locally (PC2 mixer: transmit_from_ml). */
        virtual void set_ml_listening(bool on) = 0;
        /*! A link room wants source `src`. Start it and return true, or return false if we can't. */
        virtual bool start_source(uint8_t src) = 0;
        /*! Nobody (link rooms) needs our source any more. */
        virtual void stop_source() = 0;
        /*! A Beo4 key aimed at our source from a link room (SOURCE_CONTROL telegram). */
        virtual void source_key(uint8_t src, uint8_t key) { (void)src; (void)key; }
        /*! Someone sent "all standby" on the link. */
        virtual void link_standby() {}
        /*! Local time for the clock broadcast. */
        virtual std::tm local_time();
};

class AudioMaster {
    public:
        typedef std::chrono::steady_clock clock;

        enum class BusState {
            idle,               ///< not distributing, not listening to the video master
            arbitrating,        ///< asked the video master whether the audio bus is free
            distributing,       ///< our source is on the audio bus
            listening_to_vm,    ///< we play a video master source (e.g. TV) from the bus
        };

        // Node addresses
        static constexpr uint8_t addr_all = 0x80;
        static constexpr uint8_t addr_all_link = 0x83;   ///< B&O: ALL_SOURCE_USERS
        static constexpr uint8_t addr_video_master = 0xc0;
        static constexpr uint8_t addr_audio_master = 0xc1;
        static constexpr uint8_t addr_pc_1 = 0xc2;

        // Payload types
        static constexpr uint8_t pt_link_info = 0x04;      ///< libpc2: MASTER_PRESENT
        static constexpr uint8_t pt_audio_bus = 0x08;
        static constexpr uint8_t pt_source_control = 0x0d; ///< libpc2: BEO4_KEY
        static constexpr uint8_t pt_standby_0 = 0x10;      ///< libpc2: STANDBY
        static constexpr uint8_t pt_standby_1 = 0x11;      ///< libpc2: RELEASE
        static constexpr uint8_t pt_local_source = 0x30;   ///< unnamed in B&O's table
        static constexpr uint8_t pt_clock = 0x40;          ///< B&O: PROGRAMMING_STATUS
        static constexpr uint8_t pt_link_connect = 0x45;   ///< B&O: LINK_CONNECT_COMMAND_1 (libpc2: GOTO_SOURCE)
        static constexpr uint8_t pt_link_connect_status = 0x44; ///< B&O: LINK_CONNECT_STATUS (libpc2: TRACK_INFO)
        static constexpr uint8_t pt_pri_system = 0x82;     ///< B&O: PRI_SYSTEM_STATUS (libpc2: TRACK_INFO_LONG)
        static constexpr uint8_t pt_source_status = 0x87;  ///< B&O: SOURCE_STATUS (libpc2: STATUS_INFO)
        static constexpr uint8_t pt_bus_released = 0x1c;   ///< unnamed; confirms the audio bus was handed over
        static constexpr uint8_t pt_lock_manager = 0x5c;   ///< B&O: LOCK_MANAGER (bus lock rooms take first)

        static constexpr uint8_t src_pc = 0x47;

        explicit AudioMaster(AudioMasterHost &host);

        /*! Startup broadcasts: idle source status, clock, LINK_INFO probe to the video master. */
        void start(clock::time_point now = clock::now());
        /*! Feed every Masterlink telegram received from the PC2. */
        void handle(MasterlinkTelegram &tgram, clock::time_point now = clock::now());
        /*! Call periodically (every 100-500 ms): timeouts and the periodic clock broadcast. */
        void tick(clock::time_point now = clock::now());

        /*! We now play `src` locally (e.g. a source picked on the panel). Link rooms listening
         *  to us are moved to it. */
        void set_local_source(uint8_t src);
        /*! Ask the video master for one of its sources (e.g. TV) and play it from the bus. */
        void listen_to_video_source(uint8_t src, clock::time_point now = clock::now());
        /*! Local standby: stop listening to the video master. Distribution to link rooms that
         *  still listen continues, as B&O's software does. */
        void local_standby();
        /*! "All standby": tell every node to switch off, and stop everything. */
        void all_standby(clock::time_point now = clock::now());
        /*! Take part in the LOCK_MANAGER (0x5C) bus lock (default on). The 2018 BM5 captures show no
         *  lock traffic at all, so replays of those turn it off. */
        void set_lock_participation(bool on);
        bool holds_bus_lock();

        // Introspection (tests, UI)
        BusState state();
        uint8_t current_source();
        std::set<uint8_t> listeners();

        static constexpr std::chrono::milliseconds vm_reply_timeout{1000};
        static constexpr std::chrono::seconds clock_interval{430};  ///< observed: ~7 min 10 s
        static constexpr std::chrono::milliseconds lock_timeout{500};

    private:
        // Telegram builders
        MasterlinkTelegram make(uint8_t dest, uint8_t src, uint8_t type, uint8_t ptype, uint8_t subtype,
                                std::vector<uint8_t> payload, uint8_t src_src = 0, uint8_t dest_src = 0);
        void send(MasterlinkTelegram tgram);
        void send_link_info_reply(uint8_t to);
        void send_audio_bus_status(uint8_t to);
        void send_source_status_active(uint8_t src);
        void send_source_status_stopped(uint8_t src);
        void send_source_status_idle();
        void send_connect_replies(uint8_t to, uint8_t src);
        void send_clock();

        // Flow
        void connect_request(uint8_t node, uint8_t src, clock::time_point now);
        void begin_arbitration(uint8_t src, clock::time_point now);
        void arbitration_step_link_info_ok(clock::time_point now);
        void start_distribution();
        void stop_distribution(bool announce_stopped);
        void change_source(uint8_t new_src);
        void stop_listening_to_vm(bool tell_vm);
        void with_lock(std::function<void()> action, clock::time_point now);
        void lock_acquired();
        void handle_lock(MasterlinkTelegram &t);

        AudioMasterHost &host;
        std::recursive_mutex mutex;

        BusState bus_state = BusState::idle;
        uint8_t source = 0;                 ///< source on the bus (distributing) or requested
        uint8_t local_source = 0;           ///< what we play locally, 0 = nothing
        std::set<uint8_t> listener_nodes;   ///< link rooms connected to our source
        std::set<uint8_t> pending_requesters;   ///< rooms waiting for replies during arbitration

        /*! Bus arbitration with the video master, as observed between a BeoSound 3000 and a Beosystem 3:
         *  LINK_INFO? -> AUDIO_BUS? -> (if busy) REQUEST 0x44 v3 "cur -> wanted", wait for 0x44 v7
         *  -> COMMAND STANDBY_0 "01 00 01", wait for COMMAND 0x1C -> distribute. Each wait times out. */
        enum class ArbitrationStep { none, link_info, audio_bus, change_request, claim } arbitration = ArbitrationStep::none;
        clock::time_point arbitration_deadline;
        uint8_t vm_busy_source = 0;

        bool use_lock = true;
        bool holds_lock = false;
        bool lock_pending = false;
        clock::time_point lock_deadline;
        std::vector<std::function<void()>> lock_waiters;

        uint8_t vm_source = 0;              ///< video source we asked for / listen to
        bool started = false;
        clock::time_point next_clock;
};

#endif
