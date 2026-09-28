#include "Fan.h"

Fan::Fan(uint8_t pin, bool activeLow)
    : pin_(pin),
      activeLow_(activeLow)
{
}

void Fan::begin()
{
    // The off level is driven before the pin becomes an output, so the relay
    // cannot catch a pulse while the GPIO is being configured. A fan clicking
    // on at every reset would be a confusing way to start, and the contacts
    // gain nothing from the extra cycle.
    digitalWrite(pin_, levelFor(false));
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, levelFor(false));

    state_ = false;
}

void Fan::on()
{
    // Only write on a real state change. The control loop re-commands the same
    // state every pass while the temperature sits still, and there is no reason
    // to re-drive the relay (docs/README.md section 3.4).
    if (state_)
    {
        return;
    }

    digitalWrite(pin_, levelFor(true));
    state_ = true;
}

void Fan::off()
{
    if (!state_)
    {
        return;
    }

    digitalWrite(pin_, levelFor(false));
    state_ = false;
}

uint8_t Fan::levelFor(bool on) const
{
    if (activeLow_)
    {
        return on ? LOW : HIGH;
    }

    return on ? HIGH : LOW;
}
