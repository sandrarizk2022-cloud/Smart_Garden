/**
 * Settings.h
 *
 * Every tunable number in the greenhouse lives here: control thresholds,
 * timings, calibration points and safety limits. Controllers read these
 * constants; they never define their own (docs/README.md section 15).
 *
 * Anything marked CALIBRATE must be measured on the real hardware before the
 * greenhouse is trusted unattended (docs/README.md section 19, Phase 7).
 */
#pragma once

#include <Arduino.h>

namespace Settings
{
    // =====================================================================
    // Climate control - cooling fans
    // =====================================================================
    // Two plain 2-wire DC fans, each on its own relay channel. They have no
    // control input to modulate, so cooling is on or off - there is no speed
    // to set and no duty to calibrate.

    // ---- Switching thresholds -------------------------------------------

    // At or above FAN_ON_TEMP the fans run; at or below FAN_OFF_TEMP they
    // stop. Between the two they hold whatever they were doing
    // (docs/README.md section 3.1).
    //
    // The 3-degree gap is the only thing standing between a temperature that
    // sits near the setpoint and a relay that chatters. It costs nothing to
    // widen and buys contact life, so err on the side of a larger gap rather
    // than a tighter grip on the target temperature.
    constexpr float FAN_ON_TEMP  = 30.0F;
    constexpr float FAN_OFF_TEMP = 27.0F;

    // =====================================================================
    // Climate control - ventilation hatch
    // =====================================================================

    // Same hysteresis idea for the hatch: open at VENT_TEMP_OPEN, close only
    // below VENT_TEMP_CLOSE. A servo that hunts back and forth wears out its
    // gears and the hatch mechanism.
    constexpr float VENT_TEMP_OPEN  = 29.0F;
    constexpr float VENT_TEMP_CLOSE = 27.5F;

    // Bright sun heats the greenhouse, so strong light also calls for venting.
    // Open above LIGHT_HIGH_THRESHOLD, close only below LIGHT_LOW_THRESHOLD
    // (docs/README.md section 3.3).
    constexpr float LIGHT_HIGH_THRESHOLD = 30000.0F;   // lux
    constexpr float LIGHT_LOW_THRESHOLD  = 25000.0F;   // lux

    // Servo end positions for the hatch. Constrained to 0-180 by
    // VentilationServo itself (docs/README.md section 17).
    constexpr int SERVO_CLOSED_ANGLE = 0;
    constexpr int SERVO_OPEN_ANGLE   = 90;

    // =====================================================================
    // Irrigation
    // =====================================================================

    // Watering starts below this soil moisture percentage.
    constexpr float MOISTURE_THRESHOLD = 35.0F;        // %

    // How long one watering burst runs.
    constexpr uint32_t PUMP_DURATION_MS = 5000;

    // Dead time after watering. Water needs time to spread through the soil,
    // so the probe still reads dry right after the pump stops. Without this
    // wait the controller would water again immediately and drown the plant
    // (docs/README.md section 3.7).
    constexpr uint32_t SOIL_STABILIZATION_MS = 60000;

    // Hard safety ceiling on a single pump run, checked independently of
    // PUMP_DURATION_MS. If any other part of the software misbehaves, the pump
    // still cannot stay on forever (docs/README.md section 17).
    constexpr uint32_t MAX_PUMP_RUNTIME_MS = 10000;

    // =====================================================================
    // Soil probe calibration - CALIBRATE on real hardware
    // =====================================================================

    // Raw 12-bit ADC counts (0-4095). Resistive and capacitive probes both
    // read HIGH when dry and LOW when wet, hence DRY > WET. These are
    // placeholders: measure the probe in dry medium and in water, then paste
    // the two numbers in.
    constexpr int SOIL_DRY_ADC = 3200;
    constexpr int SOIL_WET_ADC = 1300;

    // How far outside the calibrated window a reading may drift before it is
    // treated as a wiring fault rather than a measurement. A disconnected
    // probe floats near 0 or near 4095; that must never be read as extremely
    // dry soil and trigger the pump (docs/README.md section 16).
    constexpr int SOIL_FAULT_MARGIN_ADC = 400;

    // Samples averaged per reading to damp ADC noise.
    constexpr uint8_t SOIL_SAMPLE_COUNT = 8;

    // =====================================================================
    // Actuator wiring polarity
    // =====================================================================

    // Pump driver module:
    // true  = the module switches the load ON when the input is driven LOW
    //         (the common opto-isolated blue relay boards)
    // false = the module switches the load ON when the input is HIGH
    //         (logic-level MOSFET driver)
    constexpr bool PUMP_ACTIVE_LOW = true;

    // Fan relay channels, one per fan. Same convention as the pump above,
    // because they are the same kind of module:
    // true  = the channel switches its fan ON when the input is driven LOW
    // false = the channel switches its fan ON when the input is HIGH
    constexpr bool FAN_ACTIVE_LOW = true;

    // Both are still to be confirmed on the bench. Flip the lines afterwards;
    // no class needs editing.

    // =====================================================================
    // Timing - every subsystem is polled, nothing blocks
    // =====================================================================
    // (docs/README.md section 11)

    // The DHT11 physically cannot be sampled faster than about 1 Hz.
    constexpr uint32_t DHT_INTERVAL_MS   = 2000;
    constexpr uint32_t LIGHT_INTERVAL_MS = 1000;
    constexpr uint32_t SOIL_INTERVAL_MS  = 1000;

    // How often control decisions are re-evaluated.
    constexpr uint32_t CONTROL_INTERVAL_MS = 500;

    // LCD redraw rate, and how long each page stays on screen.
    constexpr uint32_t LCD_INTERVAL_MS      = 1000;
    constexpr uint32_t LCD_PAGE_INTERVAL_MS = 3000;

    // Serial telemetry line rate - the main debugging surface.
    constexpr uint32_t SERIAL_LOG_INTERVAL_MS = 2000;

    // =====================================================================
    // Sensor failure policy
    // =====================================================================

    // A failed read does not immediately invalidate a value: the last good
    // reading stays usable for this long, which rides out the occasional
    // DHT11 checksum error. After that the field is marked invalid and the
    // controllers stop trusting it (docs/README.md section 16).
    constexpr uint32_t SENSOR_STALE_TIMEOUT_MS = 15000;

    // =====================================================================
    // Measurement sanity limits
    // =====================================================================
    // Readings outside these ranges are treated as failed measurements rather
    // than as data (docs/README.md section 17).

    constexpr float TEMP_MIN_VALID = -40.0F;   // degrees C
    constexpr float TEMP_MAX_VALID =  80.0F;

    constexpr float HUMIDITY_MIN_VALID =   0.0F;   // %
    constexpr float HUMIDITY_MAX_VALID = 100.0F;

    // The BH1750 tops out around 54612 lux in high-resolution mode; the limit
    // here is deliberately loose and only catches nonsense.
    constexpr float LUX_MIN_VALID =      0.0F;
    constexpr float LUX_MAX_VALID = 120000.0F;

    // =====================================================================
    // Display hardware
    // =====================================================================

    // 0x27 is the usual PCF8574 backpack address; some boards answer at 0x3F.
    constexpr uint8_t LCD_I2C_ADDRESS = 0x27;
    constexpr uint8_t LCD_COLS        = 16;
    constexpr uint8_t LCD_ROWS        = 2;

    // =====================================================================
    // Serial
    // =====================================================================

    constexpr uint32_t SERIAL_BAUD = 115200;
}
