#include "LightSensor.h"

#include "../Config/Settings.h"

LightSensor::LightSensor(uint32_t intervalMs)
    : intervalMs_(intervalMs)
{
}

bool LightSensor::begin()
{
    // CONTINUOUS_HIGH_RES_MODE: the sensor keeps measuring on its own, so
    // readLightLevel() just fetches the newest result instead of triggering a
    // conversion and waiting for it. That keeps update() non-blocking.
    present_ = bh1750_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

    // Read immediately on the first update() rather than after a full interval.
    lastReadTime_ = millis() - intervalMs_;

    return present_;
}

bool LightSensor::update()
{
    // A sensor that never initialised is skipped entirely; its value stays
    // invalid and the ventilation rule falls back to temperature alone.
    if (!present_)
    {
        return false;
    }

    const uint32_t now = millis();

    if (now - lastReadTime_ < intervalMs_)
    {
        return false;
    }

    lastReadTime_ = now;

    const float lux = bh1750_.readLightLevel();

    // The library signals trouble with negative values (-1 for a failed I2C
    // transaction, -2 for a conversion that is not ready). The range check
    // below rejects those along with anything else implausible
    // (docs/README.md section 17).
    if (lux < Settings::LUX_MIN_VALID || lux > Settings::LUX_MAX_VALID || isnan(lux))
    {
        return false;
    }

    lux_ = lux;
    valid_ = true;

    lastValidTime_ = now;
    hasEverRead_ = true;

    return true;
}

uint32_t LightSensor::millisSinceLastValid() const
{
    if (!hasEverRead_)
    {
        return 0xFFFFFFFFU;   // never read successfully - treat as infinitely old
    }

    return millis() - lastValidTime_;
}
