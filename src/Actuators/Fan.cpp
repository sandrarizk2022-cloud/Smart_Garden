#include "Fan.h"

// The LEDC timer reserved for the fans.
//
// ESP32PWM::allocateTimer() has a trap: the *first* call marks all four timers
// as full and then frees only the one named. VentilationServo claims timer 0
// and locks it to the servo's 50 Hz, so without a second claim here the fans
// would find timer 0 at the wrong frequency, timers 1-3 marked full, and fail
// to attach. Claiming timer 1 gives the fans their own frequency; both share
// it, because one timer carries four channels.
static constexpr int FAN_LEDC_TIMER = 1;

// Speeds are expressed on this scale. 0 is a stopped fan and is handled
// separately; 1 is the slowest duty the fan still turns at.
static constexpr uint8_t MIN_RUNNING_PERCENT = 1;
static constexpr uint8_t MAX_RUNNING_PERCENT = 100;

// Resolution limits of the LEDC peripheral.
static constexpr uint8_t MIN_RESOLUTION_BITS = 1;
static constexpr uint8_t MAX_RESOLUTION_BITS = 16;

/**
 * Claims the fan timer exactly once.
 *
 * A repeat call would reset that timer's channel count while channels are
 * already attached to it, corrupting the library's bookkeeping - so the second
 * fan must not repeat it. Order against VentilationServo does not matter:
 * whichever runs first flips the library into explicit mode, and the other
 * then only frees its own timer.
 */
static void allocateFanTimerOnce()
{
    static bool allocated = false;

    if (allocated)
    {
        return;
    }

    ESP32PWM::allocateTimer(FAN_LEDC_TIMER);
    allocated = true;
}

Fan::Fan(uint8_t pin,
         uint32_t pwmFrequencyHz,
         uint8_t pwmResolutionBits,
         uint8_t minDutyPercent,
         uint8_t maxDutyPercent,
         bool inverted)
    : pin_(pin),
      frequencyHz_(pwmFrequencyHz),
      // Clamp at construction, so an out-of-range value in Settings.h can
      // never reach the peripheral - the same guard VentilationServo puts on
      // its angles (docs/README.md section 17).
      resolutionBits_(static_cast<uint8_t>(
          constrain(pwmResolutionBits, MIN_RESOLUTION_BITS, MAX_RESOLUTION_BITS))),
      minDutyPercent_(static_cast<uint8_t>(
          constrain(minDutyPercent, MIN_RUNNING_PERCENT, MAX_RUNNING_PERCENT))),
      // The window must not be inside out: a max below the min would make the
      // ramp run backwards.
      maxDutyPercent_(static_cast<uint8_t>(
          constrain(maxDutyPercent, minDutyPercent_, MAX_RUNNING_PERCENT))),
      inverted_(inverted)
{
}

bool Fan::begin()
{
    allocateFanTimerOnce();

    // Order matters. The stopped level is set *before* the pin becomes an
    // output, so the pin never spends time driving the "run" level. With an
    // inverted input a plain pinMode(OUTPUT) first would briefly drive LOW and
    // spin the fan up at every boot.
    const uint8_t stoppedLevel = inverted_ ? HIGH : LOW;

    digitalWrite(pin_, stoppedLevel);
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, stoppedLevel);

    pwm_.attachPin(pin_, static_cast<double>(frequencyHz_), resolutionBits_);

    // attached() is not proof of success: ESP32PWM sets it for any
    // PWM-capable pin, even when no LEDC channel was free, and then writes
    // would go to channel -1. A non-negative channel number is the honest
    // test, and the case is worth catching because a timer that could not be
    // claimed is otherwise a completely silent failure.
    attached_ = pwm_.attached() && (pwm_.getChannel() >= 0);

    speedPercent_ = 0;

    // Hand the pin over to the PWM peripheral in the stopped state, so the
    // software state and the fan agree from the first moment.
    if (attached_)
    {
        pwm_.write(countsFor(0));
    }

    return attached_;
}

void Fan::setSpeedPercent(uint8_t percent)
{
    if (percent > MAX_RUNNING_PERCENT)
    {
        percent = MAX_RUNNING_PERCENT;
    }

    // Write only on an actual change. The control loop re-commands the same
    // speed every pass while the temperature sits still, and there is no
    // reason to hammer the peripheral (docs/README.md section 3.4).
    if (percent == speedPercent_)
    {
        return;
    }

    speedPercent_ = percent;

    // Nothing is written before attachPin() succeeded, which keeps a
    // mis-ordered begin() from faulting.
    if (!attached_)
    {
        return;
    }

    pwm_.write(countsFor(percent));
}

uint32_t Fan::countsFor(uint8_t percent) const
{
    const uint32_t maxCount = (1UL << resolutionBits_) - 1UL;

    uint32_t dutyPercent = 0;

    if (percent > 0)
    {
        // Map the 1-100 speed onto this fan's usable window, so the caller's
        // whole range is spent where the fan actually works: speed 1 lands on
        // minDutyPercent_ and speed 100 on maxDutyPercent_.
        const uint32_t span = static_cast<uint32_t>(maxDutyPercent_ - minDutyPercent_);
        const uint32_t steps = static_cast<uint32_t>(MAX_RUNNING_PERCENT - MIN_RUNNING_PERCENT);

        dutyPercent = static_cast<uint32_t>(minDutyPercent_) +
                      ((static_cast<uint32_t>(percent - MIN_RUNNING_PERCENT) * span) / steps);
    }

    const uint32_t counts = (maxCount * dutyPercent) / 100UL;

    // The one place in the firmware that knows about signal polarity.
    return inverted_ ? (maxCount - counts) : counts;
}
