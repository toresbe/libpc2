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

#include "masterlink/protocol.hpp"
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
        /*! Ask the PC2 firmware for the Masterlink bus lock (host command 24). The firmware runs the
         *  LOCK_MANAGER protocol itself and answers IN 44 01 when granted -> AudioMaster::bus_lock_granted(). */
        virtual void request_bus_lock() {}
        /*! Release the bus lock early (host command e4 01) instead of letting the firmware's hold expire. */
        virtual void release_bus_lock() {}
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

        // Node addresses, telegram/payload types and payload layouts: see masterlink/protocol.hpp.

        /*! `system_configuration` is B&O's SystemConfiguration, i.e. the mode byte libpc2 sent in the PC2's
         *  F6 option telegram (audio-master mode sends 5, "A.OPT = 0"). B&O's software varies one LINK_INFO
         *  byte with it, so we do too. */
        static constexpr uint8_t default_system_configuration = 5;
        explicit AudioMaster(AudioMasterHost &host, uint8_t system_configuration = default_system_configuration);

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
        void local_standby(clock::time_point now = clock::now());
        /*! "All standby": tell every node to switch off, and stop everything. */
        void all_standby(clock::time_point now = clock::now());
        /*! How the LOCK_MANAGER (ml::PayloadType::lock_manager) bus lock is taken before TV / all-standby:
         *  - firmware (default): the PC2 firmware runs the lock protocol and consumes all 0x5C telegrams
         *    whenever the host started it with "80 <node> 00" (as libpc2 does). We ask with 24, wait for
         *    IN 44 01 (bus_lock_granted()), act, then release with e4 01 - exactly B&O's sequence.
         *  - software: run the 0x5C exchange ourselves. Only meaningful if the PC2 forwards 0x5C
         *    telegrams, i.e. it was started with "80 <node> <non-zero>" (promiscuous mode).
         *  - off: no lock (the 2018 BM5 captures show no lock traffic; their replays use this). */
        enum class LockMode { off, firmware, software };
        void set_lock_mode(LockMode mode);
        /*! Compatibility: true = software, false = off. */
        void set_lock_participation(bool on);
        /*! The PC2 reported IN 44 01: the firmware holds the bus lock for us. */
        void bus_lock_granted();
        bool holds_bus_lock();

        // Introspection (tests, UI)
        BusState state();
        uint8_t current_source();
        std::set<uint8_t> listeners();

        static constexpr std::chrono::milliseconds vm_reply_timeout{1000};
        static constexpr std::chrono::seconds clock_interval{430};  ///< observed: ~7 min 10 s
        static constexpr std::chrono::milliseconds lock_timeout{500};           ///< software mode
        static constexpr std::chrono::milliseconds firmware_lock_timeout{2000}; ///< firmware mode (> 1.5 s)

    private:
        // Telegram builders
        MasterlinkTelegram make(uint8_t dest, ml::Node src, ml::TelegramType type, ml::PayloadType ptype,
                                uint8_t subtype, std::vector<uint8_t> payload, uint8_t src_src = 0,
                                uint8_t dest_src = 0);
        /*! Most telegrams: from the audio master address (C1). */
        MasterlinkTelegram make(uint8_t dest, ml::TelegramType type, ml::PayloadType ptype, uint8_t subtype,
                                std::vector<uint8_t> payload = {});
        void send(MasterlinkTelegram tgram);
        void send_link_info_reply(uint8_t to);
        void send_audio_bus_status(uint8_t to);
        void send_source_status_active(uint8_t src);
        void send_source_status_stopped(uint8_t src);
        void send_source_status_idle();
        void send_connect_replies(uint8_t to, uint8_t src);
        void send_clock();
        void send_link_info_request_to_vm();
        void send_take_bus_from_vm();

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
        const uint8_t system_configuration;
        std::recursive_mutex mutex;

        BusState bus_state = BusState::idle;
        uint8_t source = 0;                 ///< source on the bus (distributing) or requested
        uint8_t local_source = 0;           ///< what we play locally, 0 = nothing
        std::set<uint8_t> listener_nodes;   ///< link rooms connected to our source
        std::set<uint8_t> pending_requesters;   ///< rooms waiting for replies during arbitration

        /*! Bus arbitration with the video master, as observed between a BeoSound 3000 and a Beosystem 3:
         *  LINK_INFO? -> AUDIO_BUS? -> (if busy) REQUEST LINK_CONNECT_COMMAND_0 DISCONNECT_SIGNAL
         *  "cur -> wanted", wait for LINK_CONNECT_STATUS DISCONNECTED_SIGNAL -> COMMAND STANDBY_0 on the
         *  audio link, wait for COMMAND 0x1C (bus released) -> distribute. Each wait times out. */
        enum class ArbitrationStep { none, link_info, audio_bus, change_request, claim } arbitration = ArbitrationStep::none;
        clock::time_point arbitration_deadline;
        uint8_t vm_busy_source = 0;

        LockMode lock_mode = LockMode::firmware;
        bool holds_lock = false;
        bool lock_pending = false;
        clock::time_point lock_deadline;
        std::vector<std::function<void()>> lock_waiters;

        uint8_t vm_source = 0;              ///< video source we asked for / listen to
        bool started = false;
        clock::time_point next_clock;
};

#endif
