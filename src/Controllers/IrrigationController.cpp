#include "IrrigationController.h"

#include "../Config/Settings.h"

IrrigationController::IrrigationController(Pump& pump)
    : pump_(pump)
{
}

void IrrigationController::begin()
{
    // The pump is commanded off explicitly rather than assumed off, so software
    // state and hardware state agree from the start.
    pump_.off();

    enterState(IrrigationState::IDLE);
}

void IrrigationController::update(float moisture, bool moistureValid)
{
    switch (state_)
    {
    case IrrigationState::IDLE:
    {
        // Two conditions, and the validity one comes first. A failed probe
        // reads at an ADC extreme, which would otherwise look like bone-dry
        // soil and start an endless watering cycle. No valid reading means no
        // watering, full stop (docs/README.md section 16).
        if (!moistureValid)
        {
            break;
        }

        if (moisture < Settings::MOISTURE_THRESHOLD)
        {
            pump_.on();
            enterState(IrrigationState::WATERING);
        }

        break;
    }

    case IrrigationState::WATERING:
    {
        // Safety cut-off, tested before the normal one and against a separate
        // constant. If PUMP_DURATION_MS were ever mis-configured, or a future
        // change introduced a path that keeps the machine in WATERING, this
        // ceiling still stops the pump (docs/README.md section 17).
        if (elapsedInState() >= Settings::MAX_PUMP_RUNTIME_MS)
        {
            pump_.off();
            enterState(IrrigationState::WAITING);
            break;
        }

        // Losing the probe mid-burst is also a reason to stop: from here on
        // there is no way to tell how wet the soil has become.
        if (!moistureValid)
        {
            pump_.off();
            enterState(IrrigationState::WAITING);
            break;
        }

        // Normal end of a watering burst.
        if (elapsedInState() >= Settings::PUMP_DURATION_MS)
        {
            pump_.off();
            enterState(IrrigationState::WAITING);
        }

        break;
    }

    case IrrigationState::WAITING:
    {
        // Belt and braces: the pump was switched off on the way into this
        // state, and it is held off here too, so no other code path can leave
        // it running through the wait.
        pump_.off();

        // Nothing is measured during the wait. Its whole purpose is to give the
        // water time to reach the probe before the next decision is made.
        if (elapsedInState() >= Settings::SOIL_STABILIZATION_MS)
        {
            enterState(IrrigationState::IDLE);
        }

        break;
    }
    }
}

void IrrigationController::stop()
{
    pump_.off();

    // Enter WAITING rather than IDLE, so an aborted cycle still serves out the
    // stabilisation period instead of immediately retrying.
    enterState(IrrigationState::WAITING);
}

const char* IrrigationController::getStateName() const
{
    switch (state_)
    {
    case IrrigationState::IDLE:     return "IDLE";
    case IrrigationState::WATERING: return "WATERING";
    case IrrigationState::WAITING:  return "WAITING";
    }

    return "UNKNOWN";
}

void IrrigationController::enterState(IrrigationState next)
{
    state_ = next;

    // Every transition restarts the clock here, in one place, so no state can
    // be entered with a stale timestamp and time out instantly.
    stateStartTime_ = millis();
}

uint32_t IrrigationController::elapsedInState() const
{
    // Unsigned subtraction stays correct across the ~49-day millis() wrap.
    return millis() - stateStartTime_;
}
