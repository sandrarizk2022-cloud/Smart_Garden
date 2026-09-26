/**
 * IrrigationController.h
 *
 * Responsibility: decide when to water, as a non-blocking finite state machine
 * (docs/README.md section 10).
 *
 *     IDLE  --(soil dry)-->  WATERING  --(burst done)-->  WAITING  --> IDLE
 *
 * The WAITING state is the point of the whole design. Water takes time to
 * spread through soil, so the probe still reads dry immediately after a burst.
 * Re-checking straight away would start another burst, and another, until the
 * plant drowns (docs/README.md section 3.7).
 *
 * No delay() anywhere: the machine is driven by millis() so the rest of the
 * firmware keeps running - the LCD keeps cycling and the fan keeps responding -
 * while the pump is on.
 */
#pragma once

#include <Arduino.h>

#include "../Actuators/Pump.h"

enum class IrrigationState
{
    IDLE,       // watching the soil, pump off
    WATERING,   // pump running
    WAITING     // pump off, letting the water spread before re-measuring
};

class IrrigationController
{
public:
    explicit IrrigationController(Pump& pump);

    // Puts the machine in IDLE with the pump confirmed off.
    void begin();

    /**
     * Advances the state machine. Safe and expected to be called often.
     *
     * @param moisture      Soil moisture in percent.
     * @param moistureValid Whether that percentage can be trusted.
     *
     * The validity flag is a required argument rather than something the caller
     * may check beforehand. Watering on a bad reading is the worst failure this
     * firmware can produce, so the guard is built into the signature and cannot
     * be forgotten at a call site (docs/README.md section 16).
     */
    void update(float moisture, bool moistureValid);

    IrrigationState getState() const { return state_; }

    // Short label for the serial log and the LCD.
    const char* getStateName() const;

    // Forces the pump off and moves to WAITING. Used for fault handling; the
    // stabilisation wait still applies afterwards, so an abort cannot turn into
    // rapid retries.
    void stop();

private:
    // Centralises the transition so every state change timestamps itself.
    void enterState(IrrigationState next);

    // Milliseconds spent in the current state.
    uint32_t elapsedInState() const;

    Pump& pump_;

    IrrigationState state_ = IrrigationState::IDLE;

    uint32_t stateStartTime_ = 0;
};
