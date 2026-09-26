#include "Pump.h"

Pump::Pump(uint8_t pin, bool activeLow)
    : pin_(pin),
      activeLow_(activeLow)
{
}

void Pump::begin()
{
    // The off level is driven before the pin is switched to an output, so the
    // pump cannot receive a pulse while the GPIO is being configured. This
    // matters more here than anywhere else in the system: a boot-time twitch on
    // the pump puts water in the pot.
    digitalWrite(pin_, levelFor(false));
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, levelFor(false));

    state_ = false;
}

void Pump::on()
{
    // Only write on a real state change, so the state machine can call on()
    // freely without re-triggering the relay.
    if (state_)
    {
        return;
    }

    digitalWrite(pin_, levelFor(true));
    state_ = true;
}

void Pump::off()
{
    if (!state_)
    {
        return;
    }

    digitalWrite(pin_, levelFor(false));
    state_ = false;
}

uint8_t Pump::levelFor(bool on) const
{
    if (activeLow_)
    {
        return on ? LOW : HIGH;
    }

    return on ? HIGH : LOW;
}
