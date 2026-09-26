/**
 * Fan.h
 *
 * Responsibility: drive one cooling fan's PWM control input, nothing else. The
 * class has no idea what temperature is - deciding how fast to run belongs to
 * ClimateController (docs/README.md section 8).
 *
 * The fan is a 5 V 3-pin unit (VCC, GND, PWM signal) with its own driver on
 * board, so there is no relay in the path: the ESP32 only produces a
 * logic-level PWM signal and the fan does the rest.
 *
 * Every fan has a usable duty window. Below some duty the rotor only hums and
 * stalls; near the top the extra noise buys almost no airflow. That window is
 * a constructor parameter and differs per fan, so callers speak in an abstract
 * 1-100 speed and this class maps it onto the duty this particular fan can
 * actually use.
 */
#pragma once

#include <Arduino.h>
#include <ESP32PWM.h>   // ships with the ESP32Servo library

class Fan
{
public:
    /**
     * @param pin               PWM-capable GPIO wired to the fan signal line.
     * @param pwmFrequencyHz    Carrier frequency.
     * @param pwmResolutionBits Duty resolution, 1-16 bits.
     * @param minDutyPercent    Slowest duty this fan still turns at.
     * @param maxDutyPercent    Fastest duty worth using.
     * @param inverted          true when 0% duty means full speed.
     *
     * The duty window is clamped and ordered at construction, so a bad value
     * in Settings.h can never reach the hardware.
     */
    Fan(uint8_t pin,
        uint32_t pwmFrequencyHz,
        uint8_t pwmResolutionBits,
        uint8_t minDutyPercent,
        uint8_t maxDutyPercent,
        bool inverted);

    /**
     * Attaches the PWM channel and leaves the fan stopped.
     *
     * @return false if no LEDC channel could be obtained, in which case this
     *         fan is inert and every later command is ignored. Reported
     *         rather than asserted, so one dead fan cannot stop the
     *         greenhouse (docs/README.md section 16).
     */
    bool begin();

    /**
     * Commands a speed.
     *
     * 0 stops the fan. 1-100 is mapped onto this fan's usable duty window, so
     * 1 is the slowest speed it can still be trusted to turn at and 100 is its
     * maximum. Values above 100 are treated as 100.
     */
    void setSpeedPercent(uint8_t percent);

    void off() { setSpeedPercent(0); }

    // Cached state, so callers never have to read the peripheral back.
    uint8_t getSpeedPercent() const { return speedPercent_; }
    bool isOn() const { return speedPercent_ > 0; }

private:
    // Turns a 0-100 speed into the raw duty count for this channel, applying
    // the usable window and the signal polarity. The one place that knows how
    // a requested speed becomes an electrical duty.
    uint32_t countsFor(uint8_t percent) const;

    ESP32PWM pwm_;

    uint8_t pin_;
    uint32_t frequencyHz_;
    uint8_t resolutionBits_;
    uint8_t minDutyPercent_;
    uint8_t maxDutyPercent_;
    bool inverted_;

    bool attached_ = false;
    uint8_t speedPercent_ = 0;
};
