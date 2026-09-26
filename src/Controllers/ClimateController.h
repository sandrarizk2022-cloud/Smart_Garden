/**
 * ClimateController.h
 *
 * Responsibility: hold the environmental rules for cooling and ventilation.
 * This is where "it is too hot" is defined; the Fan and VentilationServo
 * classes only carry the decision out (docs/README.md section 9).
 *
 * It owns no hardware. The actuators are held by reference, which keeps their
 * lifetime with GreenhouseController and makes it obvious that this class
 * commands them rather than possessing them.
 */
#pragma once

#include "../Actuators/Fan.h"
#include "../Actuators/VentilationServo.h"
#include "../Models/EnvironmentData.h"

class ClimateController
{
public:
    ClimateController(Fan& fan1, Fan& fan2, VentilationServo& ventilation);

    /**
     * Applies the climate rules to one environment snapshot.
     *
     * Cheap to call repeatedly - the actuators ignore commands that would not
     * change their state - so the caller only has to respect its own control
     * interval.
     */
    void update(const EnvironmentData& data);

private:
    /**
     * Fan rule: hysteresis between FAN_ON_TEMP and FAN_OFF_TEMP decides
     * whether the fans run at all, then a linear ramp up to FAN_FULL_TEMP
     * decides how fast.
     */
    void updateFans(const EnvironmentData& data);

    /**
     * Turns a temperature into a 1-100 speed along the ramp.
     *
     * The result is deliberately abstract: this class does not know what duty
     * either fan can actually use, and each Fan maps the number onto its own
     * usable window (Settings::FANx_MIN_DUTY_PERCENT and ..._MAX_...).
     */
    static uint8_t speedFor(float temperature);

    // Hatch rule: open on high temperature or strong light, close only when
    // both have fallen back through their lower thresholds.
    void updateVentilation(const EnvironmentData& data);

    Fan& fan1_;
    Fan& fan2_;
    VentilationServo& ventilation_;

    // Which side of the hysteresis band the fans are on. Held here rather
    // than read back from a Fan, because between FAN_OFF_TEMP and
    // FAN_ON_TEMP the decision is "whatever was decided last time" and that
    // is a property of the rule, not of the hardware.
    bool fansRunning_ = false;
};
