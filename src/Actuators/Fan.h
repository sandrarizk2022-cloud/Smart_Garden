/**
 * Fan.h
 *
 * Responsibility: switch one cooling fan's relay, nothing else. The class has
 * no idea what temperature is - deciding when to run belongs to
 * ClimateController (docs/README.md section 8).
 *
 * The fans are plain 2-wire DC units with no control input of any kind, so
 * there is nothing to modulate: each one hangs off its own relay channel and
 * cooling is simply on or off. Varying the speed would need different fans.
 *
 * Deliberately a separate class from Pump even though the code is now nearly
 * identical - the mirror of the note in Pump.h. The pump is the one actuator
 * that can cause damage if it misbehaves, and keeping the two apart leaves room
 * for pump-specific protection without touching the fan.
 */
#pragma once

#include <Arduino.h>

class Fan
{
public:
    /**
     * @param pin       GPIO wired to the relay channel input.
     * @param activeLow true when the relay switches the fan on with a LOW level.
     */
    Fan(uint8_t pin, bool activeLow);

    // Configures the pin and leaves the fan stopped.
    void begin();

    void on();
    void off();

    // Cached state, so callers never have to read the pin back.
    bool isOn() const { return state_; }

private:
    uint8_t levelFor(bool on) const;

    uint8_t pin_;
    bool activeLow_;

    bool state_ = false;
};
