/**
 * main.cpp
 *
 * Entry point only. All behaviour lives in the application classes, so this
 * file just starts the system and keeps it ticking (docs/README.md section 13).
 *
 * loop() must never block. Every subsystem schedules itself with millis(), so
 * update() returns quickly and the greenhouse stays responsive even while the
 * pump is running.
 */
#include <Arduino.h>

#include "App/GreenhouseController.h"
#include "Config/Settings.h"

// Single global instance. Constructed before setup() runs; its constructor only
// stores configuration and touches no hardware, which is what makes that safe.
GreenhouseController greenhouse;

void setup()
{
    Serial.begin(Settings::SERIAL_BAUD);

    greenhouse.begin();
}

void loop()
{
    greenhouse.update();
}
