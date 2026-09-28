/**
 * Buzzer.h
 *
 * Responsibility: make the buzzer sound or stay silent, nothing else. It does
 * not know what a pump is and holds no rule about when to sound - that pairing
 * is made by GreenhouseController, which drives this from the pump's state
 * (docs/README.md section 8).
 *
 * This is a passive buzzer: there is no oscillator inside it, so holding the
 * pin at a level produces silence, not a tone. The note has to be generated,
 * which is why this class owns an LEDC channel while Pump and Fan - which only
 * ever close a relay - need nothing but digitalWrite().
 *
 * That is also the reason it stays a separate class rather than folding into
 * Pump or Fan despite the familiar on()/off() shape: it is the only actuator
 * here that claims a hardware timer, and merging would drag that dependency
 * into two classes that are better off without it.
 */
#pragma once

#include <Arduino.h>
#include <ESP32PWM.h>   // ships with the ESP32Servo library

class Buzzer
{
public:
    /**
     * @param pin             PWM-capable GPIO wired to the buzzer driver input.
     * @param toneHz          Frequency of the generated note.
     * @param resolutionBits  LEDC duty resolution, clamped to 1-16.
     * @param activeLow       true when the buzzer sounds on a LOW level.
     */
    Buzzer(uint8_t pin, uint32_t toneHz, uint8_t resolutionBits, bool activeLow);

    /**
     * Claims an LEDC timer, configures the pin and leaves the buzzer silent.
     *
     * @return false when no LEDC channel could be claimed. Not fatal: the
     *         greenhouse keeps watering, it just does so quietly, so the caller
     *         only needs to report it (docs/README.md section 16).
     */
    bool begin();

    void on();
    void off();

    // Convenience for the caller that mirrors another actuator's state.
    void setState(bool on);

    // Cached state, so callers never have to read the pin back.
    bool isOn() const { return state_; }

private:
    // Duty that holds the pin at its inactive level, silencing the buzzer.
    uint32_t silentDuty() const;

    // Duty for an even square wave - the loudest a piezo will play.
    uint32_t soundingDuty() const;

    ESP32PWM pwm_;

    uint8_t pin_;
    uint32_t toneHz_;
    uint8_t resolutionBits_;
    bool activeLow_;

    bool state_ = false;

    // False when no LEDC channel was free; every write is then skipped.
    bool attached_ = false;
};
