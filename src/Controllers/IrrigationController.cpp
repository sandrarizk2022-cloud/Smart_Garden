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
    // Recorded before the switch, so they always describe the decision this
    // call is about to take whichever branch runs. Nothing below reads them;
    // they exist purely so describeState() can explain itself afterwards.
    lastMoisture_ = moisture;
    lastMoistureValid_ = moistureValid;

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

void IrrigationController::describeState(char* buf, size_t len) const
{
    // The label comes from getStateName() rather than being spelled again
    // here, so the two can never disagree about what a state is called.
    const char* name = getStateName();

    // Written before the switch, so a state_ that somehow falls outside the
    // enum still leaves a sane string behind instead of whatever the caller's
    // stack happened to hold.
    snprintf(buf, len, "%s", name);

    switch (state_)
    {
    case IrrigationState::IDLE:
        if (!lastMoistureValid_)
        {
            // The probe guard is what is holding the pump, not the threshold -
            // a distinction worth making, because no threshold change will
            // move this one.
            snprintf(buf, len, "%s(no-probe)", name);
        }
        else
        {
            // Both halves of the comparison, because either one explains an
            // idle pump: soil that is genuinely wet enough, or a build running
            // a threshold that is no longer the one in Settings.h.
            snprintf(buf, len, "%s(%.0f>=%.0f)",
                     name, lastMoisture_, Settings::MOISTURE_THRESHOLD);
        }
        break;

    case IrrigationState::WATERING:
        snprintf(buf, len, "%s(%lus/%lus)",
                 name,
                 static_cast<unsigned long>(elapsedInState() / 1000UL),
                 static_cast<unsigned long>(Settings::PUMP_DURATION_MS / 1000UL));
        break;

    case IrrigationState::WAITING:
    {
        // Clamped rather than subtracted blind: between two update() calls the
        // elapsed time can pass the wait before the transition runs, and the
        // unsigned underflow would report roughly 49 days remaining.
        const uint32_t elapsed = elapsedInState();
        const uint32_t remaining = (elapsed >= Settings::SOIL_STABILIZATION_MS)
                                       ? 0U
                                       : (Settings::SOIL_STABILIZATION_MS - elapsed);

        snprintf(buf, len, "%s(%lus left)",
                 name, static_cast<unsigned long>(remaining / 1000UL));
        break;
    }
    }
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
