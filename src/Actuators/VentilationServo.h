/**
 * VentilationServo.h
 *
 * Responsibility: move the ventilation hatch to its open or closed position.
 *
 * The class understands open() and close() and nothing more. It must never
 * contain a rule such as "if (temperature > 30)" - that decision belongs to
 * ClimateController, and keeping the two apart is what makes either one safe to
 * change alone (docs/README.md section 3.5).
 */
#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

class VentilationServo
{
public:
    /**
     * @param pin         PWM-capable GPIO wired to the servo signal line.
     * @param closedAngle Angle for a shut hatch.
     * @param openAngle   Angle for an open hatch.
     *
     * Both angles are clamped to the 0-180 degree mechanical range
     * (docs/README.md section 17).
     */
    VentilationServo(uint8_t pin, int closedAngle, int openAngle);

    // Attaches the servo and drives the hatch to the closed position, so the
    // software state and the physical hatch agree from the first moment.
    void begin();

    void open();
    void close();

    bool isOpen() const { return opened_; }

private:
    // Sends an angle to the servo, clamped once more as a last line of defence.
    void writeAngle(int angle);

    Servo servo_;

    uint8_t pin_;

    int closedAngle_;
    int openAngle_;

    bool opened_ = false;
    bool attached_ = false;
};
