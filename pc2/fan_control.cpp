#include "pc2/fan_control.hpp"

static int clamp_duty(int v) { return v < 0 ? 0 : (v > 0xff ? 0xff : v); }

uint8_t PC2FanControl::on_temperature(int8_t celsius) {
    int t = celsius;
    if (t < 0 || t >= this->maximum) {
        // Implausible or too hot: full duty (B&O does the same for invalid/low and high readings)
        this->fan = 0xff;
    } else if (this->fan < 0) {
        this->fan = (t > this->ideal + this->hysteresis) ? this->step_hot : 0x80;
    } else if (t > this->ideal + this->hysteresis) {
        this->fan += this->step_hot;
    } else if (t > this->previous) {
        this->fan += this->step_change;
    } else if (t < this->previous) {
        this->fan -= this->step_change;
    } else {
        this->fan -= this->step_decay;
    }
    this->fan = clamp_duty(this->fan);
    this->previous = t;
    return (uint8_t)this->fan;
}
