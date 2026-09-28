# ESP32-Based Smart Greenhouse Automation

## Software Requirements and Implementation Plan

## 1. Project Overview

The Smart Greenhouse system is an ESP32-based embedded controller designed to continuously monitor environmental conditions and automatically control greenhouse equipment.

The system monitors:

* Air temperature
* Air humidity
* Light intensity
* Soil moisture

Based on configurable thresholds, it controls:

* Cooling fans
* Ventilation hatch/window through a servo motor
* Irrigation pump

A 16×2 I2C LCD provides local real-time status information.

The system should operate autonomously and should not require manual intervention during normal operation.

---

# 2. Hardware Components

| Component            | Purpose                                  |
| -------------------- | ---------------------------------------- |
| ESP32                | Main controller                          |
| DHT11                | Air temperature and humidity measurement |
| BH1750               | Digital ambient light measurement in lux |
| Soil Moisture Sensor | Soil moisture measurement                |
| 16×2 I2C LCD         | Local status display                     |
| Cooling Fans (x2)    | Greenhouse temperature regulation        |
| Servo Motor          | Controls ventilation hatch/window        |
| Water Pump           | Irrigation                               |
| Relay Module         | Switches the two fans and the pump       |

The ESP32 should not drive any of these loads directly. Three switched channels are needed — one per fan, plus the pump — so a 4-channel relay module is the convenient part, with the spare channel left for future use.

The two cooling fans are plain 2-wire DC units. They have no control input of any kind, so there is nothing for the firmware to modulate: each fan sits on its own relay channel and is either running or stopped. Cooling is therefore on/off, and the only decision the firmware makes about the fans is whether they should run.

A channel each, rather than both fans on one channel, costs nothing at build time and leaves the option of running a single fan later without rewiring.

---

# 3. Functional Requirements

## 3.1 Temperature Monitoring

The DHT11 periodically measures greenhouse temperature.

The current temperature shall be available to other system modules and displayed on the LCD.

Example configurable thresholds:

```cpp
TEMP_FAN_ON  = 30.0 °C
TEMP_FAN_OFF = 27.0 °C
```

Using separate ON and OFF thresholds provides hysteresis and prevents the fans from repeatedly starting and stopping around one temperature. With relay-switched fans this is not merely cosmetic: every transition is a mechanical contact operation against the fans' inrush current, so the gap between the two thresholds is what protects the relay. Widening it is cheap; narrowing it buys tighter temperature control at the cost of contact life.

Example:

```text
Temperature = 31°C
→ Fans running

Temperature = 29°C
→ Fans remain running (inside the hysteresis band)

Temperature = 26°C
→ Fans stopped
```

---

# 3.2 Humidity Monitoring

The DHT11 shall also measure relative humidity.

Humidity will initially be used for monitoring and display.

It should nevertheless be exposed through the sensor architecture so it can later participate in greenhouse control decisions.

Possible future rules include:

```text
High humidity
→ Increase ventilation

Low humidity
→ Trigger humidification
```

---

# 3.3 Light Monitoring

The BH1750 shall periodically measure ambient light intensity in lux.

The measured light level will be available to the ventilation controller and displayed periodically on the LCD.

Example thresholds:

```cpp
LIGHT_HIGH_THRESHOLD = 30000 lux
LIGHT_LOW_THRESHOLD  = 25000 lux
```

The exact thresholds should remain configurable rather than hard-coded inside the controller.

---

# 3.4 Cooling Fan Control

The two cooling fans shall automatically operate according to greenhouse temperature. They are relay-switched and have no control input, so there is exactly one decision to make: whether they run.

Recommended control logic:

```text
IF temperature >= FAN_ON_THRESHOLD
    Fans running

IF temperature <= FAN_OFF_THRESHOLD
    Fans stopped

Otherwise
    Fans hold whatever they were doing
```

That third case is the whole point of having two thresholds. Between FAN_OFF_THRESHOLD and FAN_ON_THRESHOLD neither branch fires, so the previous decision stands and a temperature drifting around a single setpoint cannot make the relays chatter.

Both fans are always commanded together — the rule is about the greenhouse, not about an individual fan. They are wired to separate channels so that a future rule *could* stage them, but no such rule exists today.

The fan controller should remember its current state, so a fan is only switched on a real transition rather than being re-driven on every loop iteration.

---

# 3.5 Ventilation / Servo Control

The servo motor controls a greenhouse window, hatch, or ventilation flap.

The hatch can initially use simple OPEN/CLOSED positions.

Example:

```cpp
SERVO_CLOSED_ANGLE = 0;
SERVO_OPEN_ANGLE   = 90;
```

Opening conditions may depend on temperature, light, or both.

Example rule:

```text
IF temperature is too high
OR light intensity is above the configured threshold
    Open ventilation hatch
ELSE
    Close ventilation hatch
```

The environmental decision should be kept outside the Servo class.

The Servo class should only understand commands such as:

```cpp
servo.open();
servo.close();
```

It should not contain business rules such as:

```cpp
if (temperature > 30)
```

This separation makes the system easier to maintain.

---

# 3.6 Soil Moisture Monitoring

The soil moisture sensor continuously or periodically measures soil condition.

The raw ADC value should first be converted into a normalized moisture percentage.

Example:

```text
0%   → Very dry
100% → Very wet
```

Calibration values should be configurable:

```cpp
SOIL_DRY_ADC
SOIL_WET_ADC
```

The software then maps the ADC reading to approximately:

```text
0–100%
```

---

# 3.7 Automatic Irrigation

The irrigation system shall automatically water plants when the soil becomes too dry.

Example:

```cpp
MOISTURE_THRESHOLD = 35%;
PUMP_DURATION       = 5 seconds;
IRRIGATION_DELAY    = 60 seconds;
```

Control sequence:

```text
Moisture < threshold
        ↓
Start pump
        ↓
Pump runs for X seconds
        ↓
Stop pump
        ↓
Wait for stabilization period
        ↓
Read moisture again
```

The stabilization delay is important.

Immediately after watering, the sensor may still report dry soil because water requires time to spread through the soil. Without a waiting period, the controller may repeatedly activate the pump and overwater the plant.

The irrigation system should therefore have explicit states:

```text
IDLE
 ↓
WATERING
 ↓
WAITING
 ↓
IDLE
```

This should be implemented as a state machine rather than using blocking `delay()` calls.

---

# 4. Key Software Design Principle

Avoid implementing the project like this:

```cpp
void loop()
{
    readDHT();
    readLight();
    readMoisture();

    if (...)
        digitalWrite(...);

    if (...)
        servo.write(...);

    if (...)
        pumpOn();

    delay(...);
}
```

This works for a small prototype but becomes difficult to extend.

Instead, divide responsibilities into independent classes.

Recommended architecture:

```text
               +----------------------+
               | GreenhouseController |
               +----------+-----------+
                          |
       +------------------+------------------+
       |                  |                  |
       v                  v                  v
 SensorManager      ClimateController   IrrigationController
       |                  |                  |
       |                  |                  |
  +----+----+         +---+----+          Pump
  |    |    |         |        |
 DHT Light Soil      Fan      Servo
       |
       v
 DisplayManager
```

---

# 5. Proposed Project Structure

```text
src/
│
├── main.cpp
│
├── App/
│   ├── GreenhouseController.h
│   └── GreenhouseController.cpp
│
├── Sensors/
│   ├── DHTSensor.h
│   ├── DHTSensor.cpp
│   ├── LightSensor.h
│   ├── LightSensor.cpp
│   ├── SoilMoistureSensor.h
│   └── SoilMoistureSensor.cpp
│
├── Actuators/
│   ├── Fan.h
│   ├── Fan.cpp
│   ├── Pump.h
│   ├── Pump.cpp
│   ├── VentilationServo.h
│   └── VentilationServo.cpp
│
├── Controllers/
│   ├── ClimateController.h
│   ├── ClimateController.cpp
│   ├── IrrigationController.h
│   └── IrrigationController.cpp
│
├── Display/
│   ├── DisplayManager.h
│   └── DisplayManager.cpp
│
├── Models/
│   └── EnvironmentData.h
│
└── Config/
    ├── Pins.h
    └── Settings.h
```

This structure works particularly well with PlatformIO.

---

# 6. Environment Data Model

Instead of passing individual temperature, humidity, light, and soil values everywhere, store the latest greenhouse measurements in one structure.

```cpp
struct EnvironmentData
{
    float temperature;
    float humidity;
    float lightLux;
    float soilMoisture;

    bool temperatureValid;
    bool humidityValid;
    bool lightValid;
    bool soilValid;
};
```

Later this structure can easily be expanded:

```cpp
float co2;
float ec;
float ph;
float vpd;
```

without changing the entire program architecture.

---

# 7. Sensor Classes

Each hardware sensor should have its own class.

## DHTSensor

Responsibility:

```text
Initialize DHT11
Read temperature
Read humidity
Validate readings
```

Suggested interface:

```cpp
class DHTSensor
{
public:
    DHTSensor(uint8_t pin);

    void begin();

    bool update();

    float getTemperature() const;
    float getHumidity() const;

private:
    uint8_t pin_;

    float temperature_;
    float humidity_;

    unsigned long lastReadTime_;
};
```

The sensor should handle its own required sampling interval.

---

## LightSensor

Responsibility:

```text
Initialize BH1750
Read light level
Return lux
```

Suggested interface:

```cpp
class LightSensor
{
public:
    bool begin();

    bool update();

    float getLux() const;

private:
    float lux_;
};
```

---

## SoilMoistureSensor

Responsibility:

```text
Read ADC
Apply calibration
Convert reading into percentage
```

Suggested interface:

```cpp
class SoilMoistureSensor
{
public:
    SoilMoistureSensor(
        uint8_t pin,
        int dryValue,
        int wetValue
    );

    void begin();

    bool update();

    int getRawValue() const;
    float getPercentage() const;

private:
    uint8_t pin_;

    int dryValue_;
    int wetValue_;

    int rawValue_;
    float percentage_;
};
```

---

# 8. Actuator Classes

The actuator classes should only control hardware.

They should not decide when an actuator should run.

## Fan

```cpp
class Fan
{
public:
    Fan(uint8_t pin, bool activeLow);

    void begin();

    void on();
    void off();

    bool isOn() const;

private:
    uint8_t levelFor(bool on) const;

    uint8_t pin_;
    bool activeLow_;
    bool state_;
};
```

One instance per fan, each on its own relay channel. `begin()` drives the off level before the pin becomes an output and again afterwards, so a fan cannot click on while its GPIO is being configured.

This is deliberately almost the same class as `Pump` below. They are kept separate because the pump is the one actuator that can cause damage if it misbehaves, and that leaves room for pump-specific protection — dry-run detection, a water-level interlock — without disturbing the fans. The duplication is the price of that separation and is accepted knowingly.

---

## Pump

```cpp
class Pump
{
public:
    Pump(uint8_t pin);

    void begin();

    void on();
    void off();

    bool isOn() const;

private:
    uint8_t pin_;
    bool state_;
};
```

---

## VentilationServo

```cpp
class VentilationServo
{
public:
    VentilationServo(
        uint8_t pin,
        int closedAngle,
        int openAngle
    );

    void begin();

    void open();
    void close();

    bool isOpen() const;

private:
    uint8_t pin_;

    int openAngle_;
    int closedAngle_;

    bool opened_;
};
```

---

# 9. Climate Controller

The `ClimateController` contains environmental control rules.

It receives sensor values and decides what should happen.

```cpp
class ClimateController
{
public:
    ClimateController(
        Fan& fan1,
        Fan& fan2,
        VentilationServo& ventilation
    );

    void update(const EnvironmentData& data);

private:
    Fan& fan1_;
    Fan& fan2_;
    VentilationServo& ventilation_;

    bool fansRunning_;   // which side of the hysteresis band we are on
};
```

Example logic:

```cpp
void ClimateController::update(const EnvironmentData& data)
{
    if (data.temperature >= FAN_ON_TEMP)
    {
        fansRunning_ = true;
    }
    else if (data.temperature <= FAN_OFF_TEMP)
    {
        fansRunning_ = false;
    }

    // Each Fan ignores a command that would not change its state, so
    // these can be called every pass and the relays still only move
    // on a real transition.
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

    bool ventilationRequired =
        data.temperature >= VENT_TEMP_THRESHOLD ||
        data.lightLux >= LIGHT_THRESHOLD;

    if (ventilationRequired)
    {
        ventilation_.open();
    }
    else
    {
        ventilation_.close();
    }
}
```

Later, the decision logic could become considerably more sophisticated without touching the hardware classes.

---

# 10. Irrigation Controller

The irrigation controller should use a non-blocking finite state machine.

```cpp
enum class IrrigationState
{
    IDLE,
    WATERING,
    WAITING
};
```

Class structure:

```cpp
class IrrigationController
{
public:
    IrrigationController(Pump& pump);

    void begin();

    void update(float moisture);

    IrrigationState getState() const;

private:
    Pump& pump_;

    IrrigationState state_;

    unsigned long stateStartTime_;
};
```

Conceptual implementation:

```cpp
switch (state_)
{
case IrrigationState::IDLE:

    if (moisture < MOISTURE_THRESHOLD)
    {
        pump_.on();

        stateStartTime_ = millis();
        state_ = IrrigationState::WATERING;
    }

    break;


case IrrigationState::WATERING:

    if (millis() - stateStartTime_ >= PUMP_DURATION_MS)
    {
        pump_.off();

        stateStartTime_ = millis();
        state_ = IrrigationState::WAITING;
    }

    break;


case IrrigationState::WAITING:

    if (millis() - stateStartTime_ >= SOIL_STABILIZATION_MS)
    {
        state_ = IrrigationState::IDLE;
    }

    break;
}
```

This is substantially better than:

```cpp
pump.on();

delay(5000);

pump.off();

delay(60000);
```

because the ESP32 remains responsive while irrigation is running.

---

# 11. Timing Architecture

Every subsystem should operate using `millis()`.

For example:

```cpp
constexpr uint32_t DHT_INTERVAL_MS      = 2000;
constexpr uint32_t LIGHT_INTERVAL_MS    = 1000;
constexpr uint32_t SOIL_INTERVAL_MS     = 1000;
constexpr uint32_t LCD_INTERVAL_MS      = 1000;
constexpr uint32_t CONTROL_INTERVAL_MS  = 500;
```

The main program can then continuously call:

```cpp
controller.update();
```

without blocking.

Each module decides internally whether enough time has elapsed to perform its task.

---

# 12. Central Greenhouse Controller

The `GreenhouseController` should act as the application-level coordinator.

Example:

```cpp
class GreenhouseController
{
public:
    void begin();
    void update();

private:
    EnvironmentData environment_;

    DHTSensor dht_;
    LightSensor light_;
    SoilMoistureSensor soil_;

    Fan fan1_;
    Fan fan2_;
    Pump pump_;
    VentilationServo ventilation_;

    ClimateController climateController_;
    IrrigationController irrigationController_;

    DisplayManager display_;
};
```

Its update cycle should conceptually perform:

```text
Update sensors
      ↓
Update EnvironmentData
      ↓
Run climate controller
      ↓
Run irrigation controller
      ↓
Update display
```

Importantly, the controller should not perform low-level operations such as:

```cpp
digitalWrite()
analogRead()
servo.write()
```

Those belong inside the respective hardware classes.

---

# 13. Main.cpp

With the proposed architecture, `main.cpp` becomes extremely small.

```cpp
#include <Arduino.h>
#include "App/GreenhouseController.h"

GreenhouseController greenhouse;

void setup()
{
    Serial.begin(115200);

    greenhouse.begin();
}

void loop()
{
    greenhouse.update();
}
```

That is generally what we want.

`main.cpp` starts the application.

The application classes contain the actual system behavior.

---

# 14. LCD Display Design

The LCD should rotate between pages because four measurements plus actuator states will not comfortably fit on a 16×2 display.

Example page 1:

```text
Temp: 28.4 C
Humidity: 61%
```

Page 2:

```text
Light: 15240lx
Soil: 47%
```

Page 3:

```text
F1:ON F2:ON
Vent:ON Pmp:OFF
```

Each fan is shown separately even though the climate rule always moves them together: the two are on independent relay channels, and a disagreement between them on screen is the quickest way to spot a channel that has stopped answering. `F1:OFF F2:OFF` is 13 characters, the worst case, which leaves the hatch and the pump to share the second line.

The display manager can switch pages every few seconds.

Example interface:

```cpp
class DisplayManager
{
public:
    void begin();

    void update(
        const EnvironmentData& environment,
        bool fan1State,
        bool fan2State,
        bool pumpState,
        bool ventState
    );

private:
    uint8_t currentPage_;

    unsigned long lastPageChange_;
};
```

---

# 15. Configuration Management

Pins and thresholds should not be scattered throughout the source code.

## Pins.h

```cpp
namespace Pins
{
    constexpr uint8_t DHT = 4;
    constexpr uint8_t SOIL = 34;

    constexpr uint8_t FAN1 = 25;    // relay channel, fan 1
    constexpr uint8_t FAN2 = 33;    // relay channel, fan 2
    constexpr uint8_t PUMP = 26;
    constexpr uint8_t SERVO = 27;
}
```

## Settings.h

```cpp
namespace Settings
{
    constexpr float FAN_ON_TEMP  = 30.0F;
    constexpr float FAN_OFF_TEMP = 27.0F;

    // Relay polarity, same convention for the fans and the pump.
    constexpr bool FAN_ACTIVE_LOW  = true;
    constexpr bool PUMP_ACTIVE_LOW = true;

    constexpr float VENT_TEMP_THRESHOLD = 29.0F;

    constexpr float LIGHT_THRESHOLD = 30000.0F;

    constexpr float MOISTURE_THRESHOLD = 35.0F;

    constexpr uint32_t PUMP_DURATION_MS =
        5000;

    constexpr uint32_t SOIL_STABILIZATION_MS =
        60000;
}
```

Changing greenhouse behavior would then require editing one configuration file rather than searching through the program.

---

# 16. Sensor Failure Handling

The controller should never activate equipment based blindly on invalid sensor values.

Example:

```cpp
if (!data.temperatureValid)
{
    // Do not make normal temperature-control decisions.
}
```

Recommended policy:

```text
DHT failure
→ Keep last valid reading for a limited time
→ Report sensor error

BH1750 failure
→ Disable light-based ventilation decision

Soil sensor failure
→ Prevent automatic irrigation

LCD failure
→ Greenhouse control continues normally
```

The irrigation case is especially important.

If the moisture sensor fails and produces an extremely low reading, the controller must not continuously water the plant.

A sensor validity check should therefore be mandatory before pump activation.

---

# 17. Additional Safety Rules

The pump controller should include a maximum allowed irrigation time.

For example:

```cpp
MAX_PUMP_RUNTIME_MS = 10000;
```

Even if another software component fails, the pump should not remain continuously active.

Similarly, servo commands should be constrained:

```cpp
0° <= servo angle <= 180°
```

and sensor measurements should undergo sanity checks.

Example:

```text
Temperature: -40°C to +80°C
Humidity: 0–100%
Soil: 0–100%
Light: >= 0 lux
```

Values outside reasonable limits should be treated as invalid measurements.

---

# 18. Recommended Runtime Flow

The main runtime sequence becomes:

```text
ESP32 starts
      ↓
Initialize sensors
      ↓
Initialize actuators
      ↓
Initialize LCD
      ↓
Enter main loop
      ↓
Update DHT sensor
      ↓
Update BH1750
      ↓
Update soil moisture
      ↓
Store latest EnvironmentData
      ↓
Evaluate fan control
      ↓
Evaluate ventilation control
      ↓
Evaluate irrigation state machine
      ↓
Refresh LCD when required
      ↓
Repeat
```

No step should block the processor for significant periods.

---

# 19. Development Phases

### Phase 1 — Hardware Drivers

Implement and independently test:

```text
DHTSensor
LightSensor
SoilMoistureSensor
Fan
Pump
VentilationServo
LCD
```

At this stage there should be no automation.

Confirm each component works correctly independently.

### Phase 2 — Sensor Aggregation

Implement:

```cpp
EnvironmentData
```

and periodically collect all sensor readings.

Verify values using Serial Monitor.

### Phase 3 — Climate Automation

Implement:

```cpp
ClimateController
```

Add:

```text
Temperature → Fan
Temperature/Light → Ventilation
```

Test the thresholds and hysteresis.

### Phase 4 — Irrigation State Machine

Implement:

```cpp
IrrigationController
```

Test transitions:

```text
IDLE
→ WATERING
→ WAITING
→ IDLE
```

Pay particular attention to ensuring that no `delay()` is required.

### Phase 5 — LCD

Implement the rotating display pages.

The LCD should consume system state; it should not contain control logic.

### Phase 6 — Failure Handling

Test disconnected or invalid sensors.

Verify especially that sensor failures cannot incorrectly activate the pump.

### Phase 7 — Calibration

Calibrate:

```text
Soil dry ADC
Soil wet ADC
Temperature thresholds
Light threshold
Servo positions
Pump watering duration
Soil stabilization duration
Relay polarity (fans and pump)
```

Calibration should happen with the actual greenhouse hardware rather than relying purely on theoretical values.

The fans need no calibration of their own — they are on or off — but their two thresholds do. Set the gap between FAN_ON_TEMP and FAN_OFF_TEMP by watching the greenhouse over a full day: if the relays click more than a few times an hour while the temperature hovers near the setpoint, the gap is too narrow. Widen it rather than accepting the clicking, since every cycle is wear on the contacts.

Relay polarity is a one-time check per module. Drive a channel HIGH with nothing connected and listen: if the relay releases rather than pulls in, that module is active-low. Confirm it before the fans and pump are wired to their loads, so a wrong guess costs nothing.

---

# 20. Future Expansion

With this architecture, additional capabilities can be introduced without redesigning the core application.

Potential extensions include:

```text
Wi-Fi
MQTT
Firebase
Web dashboard
Remote configuration
Mobile application
Data logging
OTA firmware updates
CO₂ sensor
pH sensor
EC sensor
Water-level sensor
Rain sensor
Heater
Humidifier
Nebulizer
Grow lights
VPD calculation
Multiple irrigation zones
Schedules
Alarm notifications
```

For example, adding MQTT should require something like:

```cpp
MQTTManager mqtt;
```

which receives `EnvironmentData`, instead of modifying every sensor class.

---

# 21. Final Recommended Architecture

The project should use roughly four software layers:

```text
Application Layer
    GreenhouseController

Control Layer
    ClimateController
    IrrigationController

Device Layer
    DHTSensor
    LightSensor
    SoilMoistureSensor
    Fan
    Pump
    VentilationServo
    DisplayManager

Hardware / Framework Layer
    Arduino
    ESP32
    Wire
    DHT Library
    BH1750 Library
    Servo Library
    LiquidCrystal_I2C
```

The important separation is:

```text
Sensors
    Measure things

Actuators
    Perform actions

Controllers
    Make decisions

GreenhouseController
    Coordinates the system
```

That separation will make the firmware substantially easier to test, debug, modify, and extend compared with implementing all automation directly inside `loop()`.
