/**
 * SoilMoistureSensor.h
 *
 * Responsibility: sample the analog soil probe, apply the calibration, and
 * express the result as a moisture percentage (docs/README.md section 7).
 *
 * This class also carries the single most safety-relevant check in the
 * firmware. A disconnected or shorted probe reads at one extreme of the ADC
 * range; if that were reported as extremely dry soil, the irrigation
 * controller would water forever. So a reading far outside the calibrated
 * window is reported as invalid rather than as a measurement
 * (docs/README.md section 16).
 */
#pragma once

#include <Arduino.h>

class SoilMoistureSensor
{
public:
    /**
     * @param pin        ADC1 input wired to the probe (input-only pins are fine).
     * @param dryValue   Raw ADC count measured in dry medium  -> 0 %.
     * @param wetValue   Raw ADC count measured in water       -> 100 %.
     * @param intervalMs Minimum time between readings.
     */
    SoilMoistureSensor(uint8_t pin, int dryValue, int wetValue, uint32_t intervalMs);

    void begin();

    // @return true only when a fresh, valid reading was captured this call.
    bool update();

    int getRawValue() const { return rawValue_; }
    float getPercentage() const { return percentage_; }

    bool isValid() const { return valid_; }

    // Age of the last accepted reading; see DHTSensor::millisSinceLastValid().
    uint32_t millisSinceLastValid() const;

private:
    // Averages several ADC samples to damp the noise a bare probe picks up.
    int readAveragedRaw() const;

    // Rejects readings that sit well outside the calibrated dry..wet window,
    // which is what an unplugged or shorted probe looks like.
    bool isRawPlausible(int raw) const;

    uint8_t pin_;

    int dryValue_;
    int wetValue_;

    uint32_t intervalMs_;
    uint32_t lastReadTime_ = 0;
    uint32_t lastValidTime_ = 0;
    bool hasEverRead_ = false;

    int rawValue_ = 0;
    float percentage_ = 0.0F;

    bool valid_ = false;
};
