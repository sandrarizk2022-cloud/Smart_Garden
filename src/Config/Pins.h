/**
 * Pins.h
 *
 * Single source of truth for every GPIO the greenhouse firmware touches.
 * Nothing else in the project may hard-code a pin number: changing the wiring
 * must only ever require editing this file (see docs/README.md section 15).
 */
#pragma once

#include <Arduino.h>

namespace Pins
{
    // ---- Sensors -----------------------------------------------------------

    // DHT11 single-wire data line.
    constexpr uint8_t DHT = 4;

    // Analog soil moisture probe.
    // GPIO34 is input-only (no output driver) which is fine for a sensor, and
    // it belongs to ADC1. That matters: ADC2 is taken over by the radio, so a
    // soil probe on ADC2 would stop reading the moment Wi-Fi/MQTT is added
    // (docs/README.md section 20). Keep this pin on ADC1 (GPIO32-39).
    constexpr uint8_t SOIL = 34;

    // ---- Actuators ---------------------------------------------------------

    // Fan relay channel inputs, one per fan. The fans are plain 2-wire DC
    // units with nothing to signal, so the ESP32 never touches them: like the
    // pump, it only tells a relay to close (docs/README.md section 2). One
    // channel each rather than one shared channel, so a single fan can be run
    // on its own later without rewiring.
    //
    // Both are chosen from the pins that neither strap the boot mode nor emit
    // a pulse while the chip comes up - which matters more with a relay than
    // it did before, since a stray pulse now means an audible click and a
    // cycle off the contacts. GPIO33 also belongs to ADC1, which still leaves
    // GPIO32 plus the input-only 35/36/39 for any future analog sensor.
    constexpr uint8_t FAN1 = 25;
    constexpr uint8_t FAN2 = 33;

    // Pump relay / MOSFET driver input. The ESP32 never drives this load
    // directly (docs/README.md section 2).
    constexpr uint8_t PUMP = 26;

    // Servo PWM signal for the ventilation hatch.
    constexpr uint8_t SERVO = 27;

    // ---- Shared I2C bus ----------------------------------------------------

    // Both the BH1750 light sensor and the 16x2 LCD sit on this one bus, so
    // the pins are named here and Wire.begin() is called exactly once, by
    // GreenhouseController::begin().
    constexpr uint8_t I2C_SDA = 21;
    constexpr uint8_t I2C_SCL = 22;
}
