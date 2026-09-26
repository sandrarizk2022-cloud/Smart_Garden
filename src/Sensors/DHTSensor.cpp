#include "DHTSensor.h"

#include "../Config/Settings.h"

// The DHT11 is the slow, cheap member of the family. DHT22/AM2302 would work
// with the same class by changing this one constant.
static constexpr uint8_t DHT_MODEL = DHT11;

DHTSensor::DHTSensor(uint8_t pin, uint32_t intervalMs)
    : dht_(pin, DHT_MODEL),
      intervalMs_(intervalMs)
{
}

void DHTSensor::begin()
{
    dht_.begin();

    // Back-date the timer so the very first update() call reads immediately
    // instead of waiting out a full interval after boot.
    lastReadTime_ = millis() - intervalMs_;
}

bool DHTSensor::update()
{
    const uint32_t now = millis();

    // Subtracting unsigned millis() values this way is overflow-safe: when the
    // 32-bit counter wraps after ~49 days the difference is still correct.
    if (now - lastReadTime_ < intervalMs_)
    {
        return false;   // not yet time for another physical read
    }

    lastReadTime_ = now;

    // One read call per measurement; the library caches the last bus
    // transaction internally, so this is a single exchange with the sensor.
    const float temperature = dht_.readTemperature();
    const float humidity    = dht_.readHumidity();

    // Each channel is validated on its own. It is possible (if unusual) for one
    // to be sane while the other is not, and a bad humidity reading should not
    // throw away a good temperature.
    bool gotSomething = false;

    if (isTemperaturePlausible(temperature))
    {
        temperature_ = temperature;

        // Note the flag is only ever set, never cleared. It means "a real
        // reading has been captured at some point". Whether that reading is
        // still fresh enough to act on is decided by the caller using
        // millisSinceLastValid(), which keeps the stale-data policy in one
        // place instead of duplicating a timeout in every sensor class.
        temperatureValid_ = true;
        gotSomething = true;
    }

    if (isHumidityPlausible(humidity))
    {
        humidity_ = humidity;
        humidityValid_ = true;
        gotSomething = true;
    }

    if (gotSomething)
    {
        lastValidTime_ = now;
        hasEverRead_ = true;
    }

    return gotSomething;
}

uint32_t DHTSensor::millisSinceLastValid() const
{
    if (!hasEverRead_)
    {
        // No successful read yet. Report an age large enough that any
        // freshness check treats the data as unusable.
        return 0xFFFFFFFFU;
    }

    return millis() - lastValidTime_;
}

bool DHTSensor::isTemperaturePlausible(float value)
{
    // isnan() catches the disconnected-sensor case: the library returns NaN
    // when the checksum fails or the line never responds. NaN also fails every
    // comparison below, but testing it explicitly documents the intent.
    if (isnan(value))
    {
        return false;
    }

    return value >= Settings::TEMP_MIN_VALID && value <= Settings::TEMP_MAX_VALID;
}

bool DHTSensor::isHumidityPlausible(float value)
{
    if (isnan(value))
    {
        return false;
    }

    return value >= Settings::HUMIDITY_MIN_VALID && value <= Settings::HUMIDITY_MAX_VALID;
}
