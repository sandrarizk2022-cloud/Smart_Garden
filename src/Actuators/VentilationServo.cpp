#include "VentilationServo.h"

// Mechanical limits of a standard hobby servo (docs/README.md section 17).
static constexpr int SERVO_MIN_ANGLE = 0;
static constexpr int SERVO_MAX_ANGLE = 180;

// Pulse-width window in microseconds. The defaults of the ESP32Servo library
// are narrower than most SG90-class servos can actually travel, so the range is
// stated explicitly to get the full sweep.
static constexpr int SERVO_MIN_PULSE_US = 500;
static constexpr int SERVO_MAX_PULSE_US = 2400;

VentilationServo::VentilationServo(uint8_t pin, int closedAngle, int openAngle)
    : pin_(pin),
      // Clamp at construction, so an out-of-range value in Settings.h can never
      // reach the servo and stall it against its end stop.
      closedAngle_(constrain(closedAngle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE)),
      openAngle_(constrain(openAngle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE))
{
}

void VentilationServo::begin()
{
    // ESP32Servo drives the pulse train from the LEDC peripheral and needs its
    // timers allocated before attach(). Timer 0 is claimed here; the other three
    // stay free for future use.
    ESP32PWM::allocateTimer(0);

    servo_.setPeriodHertz(50);   // standard analog servo frame rate
    servo_.attach(pin_, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);

    attached_ = true;

    // Start from a known physical position rather than wherever the horn
    // happened to be left, so isOpen() is truthful immediately.
    writeAngle(closedAngle_);
    opened_ = false;
}

void VentilationServo::open()
{
    // Skip the write when already open: re-commanding the same angle only makes
    // the servo hunt and draw current for no benefit.
    if (opened_)
    {
        return;
    }

    writeAngle(openAngle_);
    opened_ = true;
}

void VentilationServo::close()
{
    if (!opened_)
    {
        return;
    }

    writeAngle(closedAngle_);
    opened_ = false;
}

void VentilationServo::writeAngle(int angle)
{
    // Nothing is written before attach() succeeded, which keeps a mis-ordered
    // begin() from faulting.
    if (!attached_)
    {
        return;
    }

    servo_.write(constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE));
}
