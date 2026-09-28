#include "ClimateController.h"

#include "../Config/Settings.h"

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

    // Both fans follow the one decision. Each Fan ignores a command that would
    // not change its state, so calling these every pass costs nothing and the
    // relays only move on a real transition.
    if (fansRunning_)
    {
        fan1_.on();
        fan2_.on();
    }
    else
    {
        fan1_.off();
        fan2_.off();
    }
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
