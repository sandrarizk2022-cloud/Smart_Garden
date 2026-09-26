#include "ClimateController.h"

#include "../Config/Settings.h"

// Speed scale handed to the fans: 0 stops them, 1 is the slowest each one
// still turns at, 100 is its maximum. Mapping these onto real duty is the
// Fan's job, because the usable window differs per fan.
static constexpr uint8_t MIN_FAN_SPEED = 1;
static constexpr uint8_t MAX_FAN_SPEED = 100;

ClimateController::ClimateController(Fan& fan1, Fan& fan2, VentilationServo& ventilation)
    : fan1_(fan1),
      fan2_(fan2),
      ventilation_(ventilation)
{
}

void ClimateController::update(const EnvironmentData& data)
{
    updateFans(data);
    updateVentilation(data);
}

void ClimateController::updateFans(const EnvironmentData& data)
{
    if (!data.temperatureValid)
    {
        // No trustworthy temperature, so hold whatever the fans are doing now.
        //
        // Holding is deliberate rather than defaulting to off: if the DHT11
        // fails on a hot afternoon while the fans are running, switching them
        // off would cook the greenhouse. Leaving them as they are keeps the
        // last known-good decision in force until a real reading returns
        // (docs/README.md section 16).
        return;
    }

    // Hysteresis: two thresholds with a gap between them. Between 27 and 30
    // degrees neither branch fires, so fansRunning_ keeps its previous value
    // and the fans do not start and stop around one temperature
    // (docs/README.md section 3.1).
    if (data.temperature >= Settings::FAN_ON_TEMP)
    {
        fansRunning_ = true;
    }
    else if (data.temperature <= Settings::FAN_OFF_TEMP)
    {
        fansRunning_ = false;
    }

    if (!fansRunning_)
    {
        fan1_.off();
        fan2_.off();
        return;
    }

    // Both fans get the same demand. They may well answer it with different
    // duties - that is exactly what their separate usable windows are for.
    const uint8_t speed = speedFor(data.temperature);

    fan1_.setSpeedPercent(speed);
    fan2_.setSpeedPercent(speed);
}

uint8_t ClimateController::speedFor(float temperature)
{
    const float span = Settings::FAN_FULL_TEMP - Settings::FAN_ON_TEMP;

    // A FAN_FULL_TEMP at or below FAN_ON_TEMP leaves no ramp to interpolate
    // over. Rather than divide by zero, treat the whole running band as "flat
    // out" - the safe reading of a configuration that says full speed is
    // wanted the moment the fans start.
    if (span <= 0.0F)
    {
        return MAX_FAN_SPEED;
    }

    const float fraction = constrain((temperature - Settings::FAN_ON_TEMP) / span,
                                     0.0F,
                                     1.0F);

    const float steps = static_cast<float>(MAX_FAN_SPEED - MIN_FAN_SPEED);

    return static_cast<uint8_t>(MIN_FAN_SPEED + lroundf(fraction * steps));
}

void ClimateController::updateVentilation(const EnvironmentData& data)
{
    // Each input contributes only while it is trustworthy. An invalid light
    // reading drops out of the expression on its own, which is exactly the
    // documented policy of disabling the light-based decision when the BH1750
    // fails - no special case needed (docs/README.md section 16).
    const bool tooHot = data.temperatureValid &&
                        data.temperature >= Settings::VENT_TEMP_OPEN;

    const bool tooBright = data.lightValid &&
                           data.lightLux >= Settings::LIGHT_HIGH_THRESHOLD;

    if (tooHot || tooBright)
    {
        ventilation_.open();
        return;
    }

    // Closing needs both reasons to have clearly gone away, each below its own
    // lower threshold. A measurement that is invalid counts as "no longer a
    // reason to vent", so a dead light sensor cannot pin the hatch open.
    const bool coolEnough = !data.temperatureValid ||
                            data.temperature < Settings::VENT_TEMP_CLOSE;

    const bool dimEnough = !data.lightValid ||
                           data.lightLux < Settings::LIGHT_LOW_THRESHOLD;

    if (coolEnough && dimEnough)
    {
        ventilation_.close();
    }

    // Anything else means at least one value is inside its hysteresis band, so
    // the hatch stays where it is.
}
