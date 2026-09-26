#include "SoilMoistureSensor.h"

#include "../Config/Settings.h"

SoilMoistureSensor::SoilMoistureSensor(uint8_t pin, int dryValue, int wetValue, uint32_t intervalMs)
    : pin_(pin),
      dryValue_(dryValue),
      wetValue_(wetValue),
      intervalMs_(intervalMs)
{
}

void SoilMoistureSensor::begin()
{
    // ESP32 ADC defaults to 12 bits (0-4095), which is what the calibration
    // constants in Settings.h assume. Stated explicitly so a future change to
    // the default cannot silently break the mapping.
    analogReadResolution(12);

    // 11 dB attenuation gives the widest input span (roughly 0-3.3 V), which is
    // what a 3.3 V-powered probe needs.
    analogSetPinAttenuation(pin_, ADC_11db);

    // Read on the first update() instead of waiting out an interval.
    lastReadTime_ = millis() - intervalMs_;
}

bool SoilMoistureSensor::update()
{
    const uint32_t now = millis();

    if (now - lastReadTime_ < intervalMs_)
    {
        return false;
    }

    lastReadTime_ = now;

    const int raw = readAveragedRaw();

    // Always publish the raw count, valid or not: it is what the operator reads
    // off the serial log when calibrating (docs/README.md section 19, Phase 7).
    rawValue_ = raw;

    if (!isRawPlausible(raw))
    {
        // Treated as a wiring fault. The stored percentage is deliberately left
        // untouched, and the caller will mark the field invalid once the value
        // goes stale - which is what blocks the pump.
        return false;
    }

    // Map the calibrated window onto 0-100 %. dryValue_ is the higher ADC count,
    // so the mapping runs "backwards" and larger counts give lower percentages.
    const float span = static_cast<float>(dryValue_ - wetValue_);

    // Guard against a mis-entered calibration collapsing the span to zero and
    // producing a division by zero.
    if (fabsf(span) < 1.0F)
    {
        return false;
    }

    float percentage = (static_cast<float>(dryValue_ - raw) / span) * 100.0F;

    // A probe slightly outside its calibration points is normal; clamping keeps
    // the published value inside the documented 0-100 % contract
    // (docs/README.md section 3.6).
    percentage = constrain(percentage, 0.0F, 100.0F);

    percentage_ = percentage;
    valid_ = true;

    lastValidTime_ = now;
    hasEverRead_ = true;

    return true;
}

uint32_t SoilMoistureSensor::millisSinceLastValid() const
{
    if (!hasEverRead_)
    {
        return 0xFFFFFFFFU;   // never read successfully - treat as infinitely old
    }

    return millis() - lastValidTime_;
}

int SoilMoistureSensor::readAveragedRaw() const
{
    uint32_t sum = 0;

    for (uint8_t i = 0; i < Settings::SOIL_SAMPLE_COUNT; ++i)
    {
        sum += static_cast<uint32_t>(analogRead(pin_));
    }

    return static_cast<int>(sum / Settings::SOIL_SAMPLE_COUNT);
}

bool SoilMoistureSensor::isRawPlausible(int raw) const
{
    // Accept the calibrated window plus a margin on each side, so a probe that
    // has drifted a little still reads. Anything beyond that is hardware
    // trouble, not soil: an open input floats toward one rail and a short pins
    // it to the other.
    const int lowerBound = wetValue_ - Settings::SOIL_FAULT_MARGIN_ADC;
    const int upperBound = dryValue_ + Settings::SOIL_FAULT_MARGIN_ADC;

    return raw >= lowerBound && raw <= upperBound;
}
