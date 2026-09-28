#include "Buzzer.h"

// The LEDC timer reserved for the buzzer.
//
// ESP32PWM::allocateTimer() has a trap: the *first* call marks all four timers
// as full and then frees only the one named. VentilationServo claims timer 0
// and locks it to the servo's 50 Hz, so without a second claim here the buzzer
// would find timer 0 at the wrong frequency, timers 1-3 marked full, and fail
// to attach - silently, which for a buzzer is a failure that looks exactly like
// success until someone stands next to the greenhouse and hears nothing.
static constexpr int BUZZER_LEDC_TIMER = 1;

// Resolution limits of the LEDC peripheral.
static constexpr uint8_t MIN_RESOLUTION_BITS = 1;
static constexpr uint8_t MAX_RESOLUTION_BITS = 16;

/**
 * Claims the buzzer timer exactly once.
 *
 * A repeat call would reset that timer's channel count while a channel is
 * already attached to it, corrupting the library's bookkeeping. Order against
 * VentilationServo does not matter: whichever runs first flips the library into
 * explicit mode, and the other then only frees its own timer.
 */
static void allocateBuzzerTimerOnce()
{
    static bool allocated = false;

    if (allocated)
    {
        return;
    }

    ESP32PWM::allocateTimer(BUZZER_LEDC_TIMER);
    allocated = true;
}

Buzzer::Buzzer(uint8_t pin, uint32_t toneHz, uint8_t resolutionBits, bool activeLow)
    : pin_(pin),
      toneHz_(toneHz),
      // Clamp at construction, so an out-of-range value in Settings.h can never
      // reach the peripheral - the same guard VentilationServo puts on its
      // angles (docs/README.md section 17).
      resolutionBits_(static_cast<uint8_t>(
          constrain(resolutionBits, MIN_RESOLUTION_BITS, MAX_RESOLUTION_BITS))),
      activeLow_(activeLow)
{
}

bool Buzzer::begin()
{
    allocateBuzzerTimerOnce();

    // Order matters. The silent level is driven before the pin becomes an
    // output, so the pin never spends time driving the "sound" level. With an
    // active-low part a plain pinMode(OUTPUT) first would drive LOW and chirp at
    // every reset.
    //
    // This shortens the boot-time window but cannot close it: until pinMode()
    // runs the pin has no output driver at all and simply floats. A buzzer
    // module without its own pull-up may therefore still be briefly audible at
    // power-on; a 10k pull-up to 3V3 on the input fixes that in hardware.
    const uint8_t silentLevel = activeLow_ ? HIGH : LOW;

    digitalWrite(pin_, silentLevel);
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, silentLevel);

    pwm_.attachPin(pin_, static_cast<double>(toneHz_), resolutionBits_);

    // attached() is not proof of success: ESP32PWM sets it for any PWM-capable
    // pin, even when no LEDC channel was free, and then writes would go to
    // channel -1. A non-negative channel number is the honest test.
    attached_ = pwm_.attached() && (pwm_.getChannel() >= 0);

    state_ = false;

    // Hand the pin over to the PWM peripheral already silent, so the software
    // state and the buzzer agree from the first moment.
    if (attached_)
    {
        pwm_.write(silentDuty());
    }

    return attached_;
}

void Buzzer::on()
{
    // Nothing is written when no channel was claimed, which keeps a failed
    // begin() from turning into writes to channel -1.
    if (!attached_ || state_)
    {
        return;
    }

    pwm_.write(soundingDuty());
    state_ = true;
}

void Buzzer::off()
{
    if (!attached_ || !state_)
    {
        return;
    }

    pwm_.write(silentDuty());
    state_ = false;
}

void Buzzer::setState(bool on)
{
    if (on)
    {
        this->on();
    }
    else
    {
        off();
    }
}

uint32_t Buzzer::silentDuty() const
{
    // Full scale is one count past the maximum duty: the counter can then
    // never reach it, so the output stays solid instead of dipping for one
    // count every cycle. ESP32PWM::write() passes the value through unclamped,
    // so it arrives intact.
    //
    // Writing the maximum duty happens to work too - the Arduino core special-
    // cases it to full on - but that is the core papering over the off-by-one
    // rather than the honest value, and it is a comparison this code should not
    // have to depend on.
    const uint32_t fullScale = 1UL << resolutionBits_;

    return activeLow_ ? fullScale : 0UL;
}

uint32_t Buzzer::soundingDuty() const
{
    // An even square wave, which is both the loudest a piezo will play and the
    // same number for either polarity - inverting a 50% duty gives 50% back.
    return 1UL << (resolutionBits_ - 1);
}
