/**
 * EnvironmentData.h
 *
 * One snapshot of the greenhouse: the latest reading for every measurement,
 * each paired with a validity flag.
 *
 * Passing this single struct around - instead of four loose floats - is what
 * lets new measurements be added later without changing any function signature
 * in the control path (docs/README.md section 6).
 *
 * The validity flags are not decoration. A controller must check the flag
 * before acting on a value, because a broken sensor still produces a number
 * rather than an error (docs/README.md section 16).
 */
#pragma once

struct EnvironmentData
{
    // ---- Measurements ------------------------------------------------------

    float temperature  = 0.0F;   // degrees C, from the DHT11
    float humidity     = 0.0F;   // % relative humidity, from the DHT11
    float lightLux     = 0.0F;   // lux, from the BH1750
    float soilMoisture = 0.0F;   // %, 0 = very dry ... 100 = very wet

    // ---- Validity ----------------------------------------------------------
    // false means: do not make decisions from the value next to this flag.
    // Defaulted to false so a snapshot taken before the first sensor read
    // completes can never be mistaken for real data.

    bool temperatureValid = false;
    bool humidityValid    = false;
    bool lightValid       = false;
    bool soilValid        = false;

    // ---- Future expansion --------------------------------------------------
    // New measurements (co2, ec, ph, vpd, water level, ...) belong here beside
    // their own validity flag. Adding one costs a field here plus a sensor
    // class; the controllers and the display keep their existing interfaces.
};
