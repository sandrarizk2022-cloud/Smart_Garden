/**
 * DisplayManager.h
 *
 * Responsibility: show system state on the 16x2 I2C LCD.
 *
 * Four measurements plus four actuator values do not fit on 32 characters, so
 * the display rotates through three pages (docs/README.md section 14).
 *
 * It consumes state and contains no control logic whatsoever. An LCD that fails
 * or is absent must not affect the greenhouse: every method degrades to doing
 * nothing (docs/README.md section 16).
 */
#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

#include "../Models/EnvironmentData.h"

class DisplayManager
{
public:
    DisplayManager(uint8_t address, uint8_t cols, uint8_t rows);

    /**
     * Initialises the LCD and shows a brief banner.
     *
     * Wire.begin() must already have been called by GreenhouseController - the
     * bus is shared with the BH1750.
     *
     * @return false if no device answered at the configured I2C address.
     */
    bool begin();

    /**
     * Redraws the current page, and advances to the next one once the page
     * interval has elapsed. Self-gated on time, so it can be called every loop.
     *
     * Actuator states are passed in rather than read from the actuators, which
     * keeps the display dependent only on plain values.
     */
    void update(const EnvironmentData& environment,
                bool fan1State,
                bool fan2State,
                bool pumpState,
                bool ventState);

private:
    // One method per page, each rendering exactly two lines.
    void renderEnvironmentPage(const EnvironmentData& environment);
    void renderLightSoilPage(const EnvironmentData& environment);
    void renderActuatorPage(bool fan1State,
                            bool fan2State,
                            bool pumpState,
                            bool ventState);

    /**
     * Writes one line, padded with spaces to the full display width.
     *
     * Padding matters: switching from a long value to a short one would
     * otherwise leave the tail of the previous text on screen. Padding each line
     * is also cheaper and far less flickery than clearing the whole display on
     * every refresh.
     */
    void writeLine(uint8_t row, const char* text);

    LiquidCrystal_I2C lcd_;

    uint8_t address_;   // kept so begin() can probe the exact configured address
    uint8_t cols_;
    uint8_t rows_;

    bool present_ = false;   // false when the LCD never initialised

    uint8_t currentPage_ = 0;
    uint32_t lastPageChange_ = 0;
    uint32_t lastRefresh_ = 0;

    // Total number of pages in the rotation.
    static constexpr uint8_t PAGE_COUNT = 3;
};
