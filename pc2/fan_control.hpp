#ifndef __PC2_FAN_CONTROL_HPP
#define __PC2_FAN_CONTROL_HPP

#include <cstdint>

/*! \brief Host side of the PC2's thermal loop (BeoMaster 5).
 *
 *  The PC2 firmware reads an I2C temperature sensor every ~30 s and reports `IN 1c T ff` (T = signed
 *  whole degrees C). The host answers with `f9 xx`, the duty of the fan PWM ((xx >> 3) / 32). If no
 *  f9 arrives for 30-60 s the firmware falls back to almost full duty, so a host that ignores this
 *  loop runs the fan at ~94 % from power-up (reports/pc2-firmware.md §4).
 *
 *  B&O's BeoIpcML ("BM5FAN-Control", registry Software\\Bang & Olufsen\\BeoLinkMedia) also uses the
 *  PC's CPU/HDD temperatures, so its exact output can't be reproduced from the case temperature alone.
 *  This is an approximation fitted to its observed behaviour, with its registry defaults
 *  (IdealTemp 37 C, HysTemp 5, MaxTemp 47 C): above Ideal+Hys the duty climbs by 0x19 per report,
 *  a rising temperature adds 0x0c, a falling one removes 0x0c, otherwise it decays by 5; at or above
 *  Max, or for implausible readings, it goes to full. Start value 0x80 (0x19 if already hot).
 */
class PC2FanControl {
    public:
        int ideal = 37;          ///< degrees C
        int hysteresis = 5;      ///< degrees C
        int maximum = 47;        ///< degrees C: at or above this, full duty
        int step_hot = 0x19;
        int step_change = 0x0c;
        int step_decay = 5;

        /*! Feed one IN 1c temperature; returns the f9 duty to send. The first report after the host
         *  opens the device is stale (firmware detail) - call skip_next() after (re)opening. */
        uint8_t on_temperature(int8_t celsius);
        void skip_next() { this->skip = true; }
        /*! True (once) if this report should be ignored as stale; clears the flag. */
        bool consume_skip() { bool s = this->skip; this->skip = false; return s; }
        int duty() const { return this->fan; }

    private:
        int fan = -1;
        int previous = 0;
        bool skip = true;
};

#endif
