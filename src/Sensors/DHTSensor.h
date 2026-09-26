/**
 * DHTSensor.h
 *
 * Responsibility: own the DHT11, read air temperature and humidity, and decide
 * whether each reading is trustworthy. Nothing more - it holds no thresholds
 * and controls no equipment (docs/README.md section 7).
 *
 * The class enforces its own sampling interval, so the caller is free to invoke
 * update() on every pass of the main loop.
 */
#pragma once

#include <Arduino.h>
#include <DHT.h>

class DHTSensor
{
public:
    /**
     * @param pin       GPIO wired to the DHT11 data line.
     * @param intervalMs Minimum time between physical reads. The DHT11 cannot
     *                   be sampled faster than roughly once per second.
     */
    DHTSensor(uint8_t pin, uint32_t intervalMs);

    // Starts the sensor. Safe to call once from setup.
    void begin();

    /**
     * Reads the sensor if the sampling interval has elapsed.
     *
     * @return true only when a fresh, valid reading was taken during this
     *         call. false means either "too soon" or "the read failed" - use
     *         isTemperatureValid() / isHumidityValid() to tell those apart.
     */
    bool update();

    // Last accepted readings. Meaningless unless the matching valid flag is set.
    float getTemperature() const { return temperature_; }
    float getHumidity() const { return humidity_; }

    bool isTemperatureValid() const { return temperatureValid_; }
    bool isHumidityValid() const { return humidityValid_; }

    /**
     * Age of the last successful read, in milliseconds. The caller uses this
     * to apply the stale-data policy: keep trusting a value for a short while
     * after a failed read, then give up on it (docs/README.md section 16).
     *
     * Returns a very large value while no successful read has happened yet.
     */
    uint32_t millisSinceLastValid() const;

private:
    // Range check from docs/README.md section 17. A DHT11 that has been
    // unplugged reports NaN, and a failing one can report wild numbers; both
    // must be rejected before they reach the control logic.
    static bool isTemperaturePlausible(float value);
    static bool isHumidityPlausible(float value);

    DHT dht_;                        // Adafruit driver instance

    uint32_t intervalMs_;            // minimum spacing between reads
    uint32_t lastReadTime_ = 0;      // millis() of the last attempted read
    uint32_t lastValidTime_ = 0;     // millis() of the last accepted reading
    bool hasEverRead_ = false;       // guards lastValidTime_ before first success

    float temperature_ = 0.0F;
    float humidity_ = 0.0F;

    bool temperatureValid_ = false;
    bool humidityValid_ = false;
};
