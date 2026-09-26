/**
 * Pump.h
 *
 * Responsibility: switch the irrigation pump relay, nothing else. It holds no
 * moisture thresholds and no timers - when and how long to water is decided by
 * IrrigationController (docs/README.md section 8).
 *
 * Deliberately a separate class from Fan even though the code is nearly
 * identical: the pump is the one actuator that can cause damage if it misbehaves,
 * and keeping it distinct leaves room for pump-specific protection (dry-run
 * detection, water-level interlock) without touching the fan.
 */
#pragma once

#include <Arduino.h>

class Pump
{
public:
    /**
     * @param pin       GPIO wired to the relay or driver input.
     * @param activeLow true when the load switches on with a LOW level.
     */
    Pump(uint8_t pin, bool activeLow);

    // Configures the pin and leaves the pump off.
    void begin();

    void on();
    void off();

    bool isOn() const { return state_; }

private:
    uint8_t levelFor(bool on) const;

    uint8_t pin_;
    bool activeLow_;

    bool state_ = false;
};
