/**
 * LightSensor.h
 *
 * Responsibility: own the BH1750 on the I2C bus and report ambient light in
 * lux. It does not decide what a given light level means - that belongs to
 * ClimateController (docs/README.md section 7).
 *
 * If the device is absent, begin() reports failure and the ventilation logic
 * simply stops taking light into account (docs/README.md section 16).
 */
#pragma once

#include <Arduino.h>
#include <BH1750.h>

class LightSensor
{
public:
    explicit LightSensor(uint32_t intervalMs);

    /**
     * Initialises the BH1750 in continuous high-resolution mode.
     *
     * Wire.begin() must already have been called - the bus is shared with the
     * LCD, so it is started once by GreenhouseController.
     *
     * @return false if the sensor does not answer on the bus.
     */
    bool begin();

    // @return true only when a fresh, valid lux value was captured this call.
    bool update();

    float getLux() const { return lux_; }

    bool isValid() const { return valid_; }

    // Age of the last accepted reading; see DHTSensor::millisSinceLastValid().
    uint32_t millisSinceLastValid() const;

    // Whether the device was found during begin(). A sensor that never
    // initialised is not polled at all.
    bool isPresent() const { return present_; }

private:
    BH1750 bh1750_;

    uint32_t intervalMs_;
    uint32_t lastReadTime_ = 0;
    uint32_t lastValidTime_ = 0;
    bool hasEverRead_ = false;

    bool present_ = false;

    float lux_ = 0.0F;
    bool valid_ = false;
};
