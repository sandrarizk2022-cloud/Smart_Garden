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
      buzzer_(Pins::BUZZER,
              Settings::BUZZER_TONE_HZ,
              Settings::BUZZER_RESOLUTION_BITS,
              Settings::BUZZER_ACTIVE_LOW),
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

    // Which build is talking, and the numbers it is actually going to use.
    // A threshold edited in Settings.h but never linked or flashed looks
    // exactly like a broken state machine from the serial log, so the firmware
    // names itself and its settings before it reports anything else.
    logBuildBanner();

    // The actuators come up first, so the fans, pump, buzzer and hatch are in a
    // known safe state before anything else can take time. Every one of them
    // drives its off level before its GPIO becomes an output, so nothing
    // twitches while the pins are being configured.
    fan1_.begin();
    fan2_.begin();
    pump_.begin();
    ventilation_.begin();

    // A buzzer that cannot claim an LEDC channel is not fatal - the greenhouse
    // waters just as well in silence - but it must be reported, because a
    // buzzer that never sounds is otherwise indistinguishable from one that
    // simply has not been asked to (docs/README.md section 16).
    if (buzzer_.begin())
    {
        Serial.println(F("[init] Actuators ready (fans stopped, pump off, "
                         "hatch closed, buzzer silent)"));
    }
    else
    {
        Serial.println(F("[init] Actuators ready (fans stopped, pump off, "
                         "hatch closed)"));
        Serial.println(F("[init] Buzzer got no PWM channel - watering will be silent"));
    }

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

    // 5. Sound the buzzer for exactly as long as the pump runs. The state is
    //    derived from the pump rather than commanded alongside it, so the two
    //    cannot drift apart, and the pump stays the single source of truth.
    //
    //    Done every pass instead of on the control interval: the call is
    //    idempotent, so it costs nothing, and the buzzer then follows the pump
    //    within one loop pass rather than up to CONTROL_INTERVAL_MS later.
    buzzer_.setState(pump_.isOn());

    // 6. Refresh the display. Self-gated on its own interval.
    display_.update(environment_,
                    fan1_.isOn(),
                    fan2_.isOn(),
                    pump_.isOn(),
                    ventilation_.isOpen());

    // 7. Periodic telemetry over Serial.
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

void GreenhouseController::logBuildBanner()
{
    Serial.print(F("[init] Build "));
    Serial.print(F(__DATE__));
    Serial.print(' ');
    Serial.println(F(__TIME__));

    char banner[120];

    // The irrigation rule in numbers. These are the values compiled into this
    // image, not the values in the editor - which is the entire point.
    snprintf(banner, sizeof(banner),
             "[init] Irrigation: water below %.1f%% | burst %lu ms | "
             "wait %lu ms | ceiling %lu ms",
             Settings::MOISTURE_THRESHOLD,
             static_cast<unsigned long>(Settings::PUMP_DURATION_MS),
             static_cast<unsigned long>(Settings::SOIL_STABILIZATION_MS),
             static_cast<unsigned long>(Settings::MAX_PUMP_RUNTIME_MS));

    Serial.println(banner);

    // The window a raw count has to land in to be read as soil rather than as
    // a wiring fault - the other thing that silently holds the pump off. The
    // bounds are derived from the same constants SoilMoistureSensor checks
    // against, so the banner cannot drift away from the check.
    snprintf(banner, sizeof(banner),
             "[init] Soil calib: dry=%d wet=%d | valid raw %d..%d",
             Settings::SOIL_DRY_ADC,
             Settings::SOIL_WET_ADC,
             Settings::SOIL_WET_ADC - Settings::SOIL_FAULT_MARGIN_ADC,
             Settings::SOIL_DRY_ADC + Settings::SOIL_FAULT_MARGIN_ADC);

    Serial.println(banner);
}

void GreenhouseController::logTelemetry()
{
    const uint32_t now = millis();

    if (now - lastLogTime_ < Settings::SERIAL_LOG_INTERVAL_MS)
    {
        return;
    }

    lastLogTime_ = now;

    char line[208];

    // Not just which state irrigation is in, but why it is there: the
    // comparison holding the pump off, or the time left to run.
    char irrigation[IrrigationController::STATE_DETAIL_SIZE];
    irrigationController_.describeState(irrigation, sizeof(irrigation));

    // One line per sample, everything needed to verify the system on the bench:
    // the readings, which of them are trusted, the raw ADC count for soil
    // calibration, and the state of every actuator.
    snprintf(line, sizeof(line),
             "T=%.1fC(%c) H=%.0f%%(%c) L=%.0flx(%c) S=%.0f%%(%c raw=%d) | "
             "F1=%s F2=%s Vent=%s Pump=%s Buzz=%s Irrig=%s",
             environment_.temperature,  environment_.temperatureValid ? 'v' : 'x',
             environment_.humidity,     environment_.humidityValid    ? 'v' : 'x',
             environment_.lightLux,     environment_.lightValid       ? 'v' : 'x',
             environment_.soilMoisture, environment_.soilValid        ? 'v' : 'x',
             soil_.getRawValue(),
             fan1_.isOn() ? "ON" : "OFF",
             fan2_.isOn() ? "ON" : "OFF",
             ventilation_.isOpen() ? "OPEN" : "SHUT",
             pump_.isOn() ? "ON" : "OFF",
             // Redundant with Pump by construction, which is the point: it
             // separates a broken mirror from a miswired buzzer on the bench.
             buzzer_.isOn() ? "ON" : "OFF",
             irrigation);

    Serial.println(line);
}

bool GreenhouseController::isFresh(bool everValid, uint32_t age)
{
    // A sensor that has never produced a good reading is never fresh; one that
    // has is trusted until its reading ages past the timeout.
    return everValid && age < Settings::SENSOR_STALE_TIMEOUT_MS;
}
