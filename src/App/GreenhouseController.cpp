#include "GreenhouseController.h"

#include <Wire.h>

#include "../Config/Pins.h"
#include "../Config/Settings.h"

GreenhouseController::GreenhouseController()
    // Every object is constructed straight from Pins and Settings. This list is
    // the complete wiring description of the system - nothing below reaches for
    // a pin number or a threshold of its own.
    : dht_(Pins::DHT, Settings::DHT_INTERVAL_MS),
      light_(Settings::LIGHT_INTERVAL_MS),
      soil_(Pins::SOIL,
            Settings::SOIL_DRY_ADC,
            Settings::SOIL_WET_ADC,
            Settings::SOIL_INTERVAL_MS),
      fan1_(Pins::FAN1, Settings::FAN_ACTIVE_LOW),
      fan2_(Pins::FAN2, Settings::FAN_ACTIVE_LOW),
      pump_(Pins::PUMP, Settings::PUMP_ACTIVE_LOW),
      ventilation_(Pins::SERVO,
                   Settings::SERVO_CLOSED_ANGLE,
                   Settings::SERVO_OPEN_ANGLE),
      climateController_(fan1_, fan2_, ventilation_),
      irrigationController_(pump_),
      display_(Settings::LCD_I2C_ADDRESS, Settings::LCD_COLS, Settings::LCD_ROWS)
{
}

void GreenhouseController::begin()
{
    Serial.println();
    Serial.println(F("=== Smart Greenhouse starting ==="));

    // The actuators come up first, so the fans, pump and hatch are in a known
    // safe state before anything else can take time. Every one of them drives
    // its off level before its GPIO becomes an output, so nothing twitches
    // while the pins are being configured.
    fan1_.begin();
    fan2_.begin();
    pump_.begin();
    ventilation_.begin();
    Serial.println(F("[init] Actuators ready (fans stopped, pump off, hatch closed)"));

    // One Wire.begin() for the whole system: the BH1750 and the LCD share the
    // bus, so neither device class starts it.
    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);

    dht_.begin();
    Serial.println(F("[init] DHT11 started"));

    // A missing BH1750 is not fatal. Ventilation simply stops considering light
    // and falls back to temperature alone (docs/README.md section 16).
    if (light_.begin())
    {
        Serial.println(F("[init] BH1750 found"));
    }
    else
    {
        Serial.println(F("[init] BH1750 NOT found - light-based venting disabled"));
    }

    soil_.begin();
    Serial.println(F("[init] Soil probe started"));

    // A missing LCD is not fatal either; control continues headless.
    if (display_.begin())
    {
        Serial.println(F("[init] LCD ready"));
    }
    else
    {
        Serial.println(F("[init] LCD NOT found - continuing without display"));
    }

    irrigationController_.begin();

    // Back-date both schedulers so the first loop pass acts immediately instead
    // of idling through one interval.
    const uint32_t now = millis();
    lastControlTime_ = now - Settings::CONTROL_INTERVAL_MS;
    lastLogTime_ = now - Settings::SERIAL_LOG_INTERVAL_MS;

    Serial.println(F("=== Running ==="));
}

void GreenhouseController::update()
{
    // 1. Poll the sensors. Each one enforces its own sampling interval
    //    internally, so calling them every pass costs almost nothing and no
    //    scheduling logic is duplicated here (docs/README.md section 11).
    dht_.update();
    light_.update();
    soil_.update();

    // 2. Collect the readings into one snapshot.
    refreshEnvironment();

    // 3. and 4. Climate rules, then the irrigation state machine.
    runControllers();

    // 5. Refresh the display. Self-gated on its own interval.
    display_.update(environment_,
                    fan1_.isOn(),
                    fan2_.isOn(),
                    pump_.isOn(),
                    ventilation_.isOpen());

    // 6. Periodic telemetry over Serial.
    logTelemetry();
}

void GreenhouseController::refreshEnvironment()
{
    // Values are copied unconditionally, but each validity flag is recomputed
    // from how recently that sensor last succeeded. The effect is that a brief
    // failure - a DHT11 checksum error, say - rides through on the last good
    // reading, while a sensor that stays broken is marked invalid and the
    // controllers stop acting on it (docs/README.md section 16).

    const uint32_t dhtAge = dht_.millisSinceLastValid();

    environment_.temperature = dht_.getTemperature();
    environment_.temperatureValid = isFresh(dht_.isTemperatureValid(), dhtAge);

    environment_.humidity = dht_.getHumidity();
    environment_.humidityValid = isFresh(dht_.isHumidityValid(), dhtAge);

    environment_.lightLux = light_.getLux();
    environment_.lightValid = isFresh(light_.isValid(), light_.millisSinceLastValid());

    environment_.soilMoisture = soil_.getPercentage();
    environment_.soilValid = isFresh(soil_.isValid(), soil_.millisSinceLastValid());
}

void GreenhouseController::runControllers()
{
    const uint32_t now = millis();

    if (now - lastControlTime_ < Settings::CONTROL_INTERVAL_MS)
    {
        return;
    }

    lastControlTime_ = now;

    // Climate first: fan and hatch react to the current snapshot.
    climateController_.update(environment_);

    // Then irrigation. The validity flag is handed over explicitly, so the state
    // machine can refuse to water on an untrustworthy reading - the check that
    // keeps a broken probe from flooding the pot.
    irrigationController_.update(environment_.soilMoisture, environment_.soilValid);
}

void GreenhouseController::logTelemetry()
{
    const uint32_t now = millis();

    if (now - lastLogTime_ < Settings::SERIAL_LOG_INTERVAL_MS)
    {
        return;
    }

    lastLogTime_ = now;

    char line[160];

    // One line per sample, everything needed to verify the system on the bench:
    // the readings, which of them are trusted, the raw ADC count for soil
    // calibration, and the state of every actuator.
    snprintf(line, sizeof(line),
             "T=%.1fC(%c) H=%.0f%%(%c) L=%.0flx(%c) S=%.0f%%(%c raw=%d) | "
             "F1=%s F2=%s Vent=%s Pump=%s Irrig=%s",
             environment_.temperature,  environment_.temperatureValid ? 'v' : 'x',
             environment_.humidity,     environment_.humidityValid    ? 'v' : 'x',
             environment_.lightLux,     environment_.lightValid       ? 'v' : 'x',
             environment_.soilMoisture, environment_.soilValid        ? 'v' : 'x',
             soil_.getRawValue(),
             fan1_.isOn() ? "ON" : "OFF",
             fan2_.isOn() ? "ON" : "OFF",
             ventilation_.isOpen() ? "OPEN" : "SHUT",
             pump_.isOn() ? "ON" : "OFF",
             irrigationController_.getStateName());

    Serial.println(line);
}

bool GreenhouseController::isFresh(bool everValid, uint32_t age)
{
    // A sensor that has never produced a good reading is never fresh; one that
    // has is trusted until its reading ages past the timeout.
    return everValid && age < Settings::SENSOR_STALE_TIMEOUT_MS;
}
