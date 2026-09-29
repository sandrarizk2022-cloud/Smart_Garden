/**
 * GreenhouseController.h
 *
 * Responsibility: coordinate the system. It owns every sensor, actuator,
 * controller and the display, wires them together, and runs the update cycle
 * (docs/README.md section 12).
 *
 * What it must never do is touch hardware directly. There is no digitalWrite(),
 * analogRead() or servo.write() in this class - those live in the device layer.
 * Its job is sequencing and the flow of data between layers.
 *
 * Update cycle (docs/README.md section 18):
 *
 *     read sensors -> build EnvironmentData -> climate rules
 *                  -> irrigation state machine -> buzzer mirrors pump
 *                  -> display -> serial log
 */
#pragma once

#include <Arduino.h>

#include "../Actuators/Buzzer.h"
#include "../Actuators/Fan.h"
#include "../Actuators/Pump.h"
#include "../Actuators/VentilationServo.h"
#include "../Controllers/ClimateController.h"
#include "../Controllers/IrrigationController.h"
#include "../Display/DisplayManager.h"
#include "../Models/EnvironmentData.h"
#include "../Sensors/DHTSensor.h"
#include "../Sensors/LightSensor.h"
#include "../Sensors/SoilMoistureSensor.h"

class GreenhouseController
{
public:
    // All wiring and configuration is bound here, in one constructor, from the
    // Pins and Settings namespaces.
    GreenhouseController();

    // Brings up the I2C bus, the sensors, the actuators and the LCD, in the
    // documented order, reporting each result over Serial.
    void begin();

    // One non-blocking pass of the whole system. Called from loop().
    void update();

private:
    // Copies the sensors into environment_ and applies the stale-reading policy.
    void refreshEnvironment();

    // Runs the climate rules and the irrigation state machine, rate-limited to
    // CONTROL_INTERVAL_MS.
    void runControllers();

    // Prints the build stamp and the control constants this image was compiled
    // with, once at startup. Without it the serial log cannot distinguish a
    // misbehaving controller from a source change that was never flashed.
    void logBuildBanner();

    // One-line telemetry: readings, validity, actuator states, irrigation state.
    void logTelemetry();

    /**
     * Applies the stale-reading policy in one place: a value stays usable for
     * SENSOR_STALE_TIMEOUT_MS after its last successful read, and is invalid
     * after that (docs/README.md section 16).
     *
     * @param everValid whether the sensor has ever produced a good reading
     * @param age       milliseconds since that reading
     */
    static bool isFresh(bool everValid, uint32_t age);

    // ---- Latest system state ----------------------------------------------

    EnvironmentData environment_;

    // ---- Device layer ------------------------------------------------------

    DHTSensor dht_;
    LightSensor light_;
    SoilMoistureSensor soil_;

    Fan fan1_;
    Fan fan2_;
    Pump pump_;

    // Declared next to the pump because it does nothing but follow it.
    Buzzer buzzer_;

    VentilationServo ventilation_;

    // ---- Control layer -----------------------------------------------------
    // Declared after the actuators on purpose: these hold references to them,
    // and members are initialised in declaration order.

    ClimateController climateController_;
    IrrigationController irrigationController_;

    // ---- Presentation ------------------------------------------------------

    DisplayManager display_;

    // ---- Scheduling --------------------------------------------------------

    uint32_t lastControlTime_ = 0;
    uint32_t lastLogTime_ = 0;
};
