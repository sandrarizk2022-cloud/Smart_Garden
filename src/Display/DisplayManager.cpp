#include "DisplayManager.h"

#include <Wire.h>

#include "../Config/Settings.h"

// Enough for a full 16-character line plus the terminator, with headroom.
static constexpr size_t LINE_BUFFER_SIZE = 24;

DisplayManager::DisplayManager(uint8_t address, uint8_t cols, uint8_t rows)
    : lcd_(address, cols, rows),
      address_(address),
      cols_(cols),
      rows_(rows)
{
}

bool DisplayManager::begin()
{
    // LiquidCrystal_I2C writes blindly and never reports an error, so the
    // backpack is probed directly first. Without this the firmware could not
    // tell the operator that the display is missing.
    Wire.beginTransmission(address_);
    present_ = (Wire.endTransmission() == 0);

    if (!present_)
    {
        return false;
    }

    lcd_.init();
    lcd_.backlight();
    lcd_.clear();

    // Brief banner so it is obvious at a glance that the display works and the
    // firmware has started.
    writeLine(0, "Smart Greenhouse");
    writeLine(1, "Starting...");

    const uint32_t now = millis();
    lastPageChange_ = now;
    lastRefresh_ = now;

    return true;
}

void DisplayManager::update(const EnvironmentData& environment,
                            uint8_t fan1Percent,
                            uint8_t fan2Percent,
                            bool pumpState,
                            bool ventState)
{
    // A missing or failed LCD costs nothing: the greenhouse carries on without
    // a display (docs/README.md section 16).
    if (!present_)
    {
        return;
    }

    const uint32_t now = millis();

    // Page rotation is timed separately from redrawing, so values on the current
    // page stay live while that page is on screen.
    if (now - lastPageChange_ >= Settings::LCD_PAGE_INTERVAL_MS)
    {
        currentPage_ = static_cast<uint8_t>((currentPage_ + 1) % PAGE_COUNT);
        lastPageChange_ = now;

        // Force an immediate redraw so the new page appears the moment it is
        // selected rather than up to a refresh interval later.
        lastRefresh_ = now - Settings::LCD_INTERVAL_MS;
    }

    if (now - lastRefresh_ < Settings::LCD_INTERVAL_MS)
    {
        return;
    }

    lastRefresh_ = now;

    switch (currentPage_)
    {
    case 0:
        renderEnvironmentPage(environment);
        break;

    case 1:
        renderLightSoilPage(environment);
        break;

    default:
        renderActuatorPage(fan1Percent, fan2Percent, pumpState, ventState);
        break;
    }
}

void DisplayManager::renderEnvironmentPage(const EnvironmentData& environment)
{
    char line[LINE_BUFFER_SIZE];

    // Invalid readings print as dashes. Showing a stale number would be worse
    // than showing nothing, because the operator could not tell the difference.
    if (environment.temperatureValid)
    {
        snprintf(line, sizeof(line), "Temp: %.1f C", environment.temperature);
    }
    else
    {
        snprintf(line, sizeof(line), "Temp: --");
    }

    writeLine(0, line);

    if (environment.humidityValid)
    {
        snprintf(line, sizeof(line), "Humidity: %.0f%%", environment.humidity);
    }
    else
    {
        snprintf(line, sizeof(line), "Humidity: --");
    }

    writeLine(1, line);
}

void DisplayManager::renderLightSoilPage(const EnvironmentData& environment)
{
    char line[LINE_BUFFER_SIZE];

    // Lux is printed without decimals; full sun is five digits and the label
    // plus "lx" already uses most of the line.
    if (environment.lightValid)
    {
        snprintf(line, sizeof(line), "Light: %.0flx", environment.lightLux);
    }
    else
    {
        snprintf(line, sizeof(line), "Light: --");
    }

    writeLine(0, line);

    if (environment.soilValid)
    {
        snprintf(line, sizeof(line), "Soil: %.0f%%", environment.soilMoisture);
    }
    else
    {
        snprintf(line, sizeof(line), "Soil: --");
    }

    writeLine(1, line);
}

void DisplayManager::renderActuatorPage(uint8_t fan1Percent,
                                       uint8_t fan2Percent,
                                       bool pumpState,
                                       bool ventState)
{
    char line[LINE_BUFFER_SIZE];

    // The fans are no longer on or off, so their speed is what is worth
    // showing. "F1:100% F2:100%" is 15 characters, the worst case on a
    // 16-column display.
    snprintf(line, sizeof(line), "F1:%u%% F2:%u%%",
             static_cast<unsigned>(fan1Percent),
             static_cast<unsigned>(fan2Percent));

    writeLine(0, line);

    // That pushes the hatch and the pump onto the second line together.
    // Abbreviated labels so "Vent:OFF Pmp:OFF" fits in exactly 16 characters.
    snprintf(line, sizeof(line), "Vent:%s Pmp:%s",
             ventState ? "ON" : "OFF",
             pumpState ? "ON" : "OFF");

    writeLine(1, line);
}

void DisplayManager::writeLine(uint8_t row, const char* text)
{
    if (!present_ || row >= rows_)
    {
        return;
    }

    lcd_.setCursor(0, row);

    // Print the text, truncated if it somehow exceeds the display width, then
    // blank the rest of the row so no characters survive from the previous page.
    uint8_t written = 0;

    while (text[written] != '\0' && written < cols_)
    {
        lcd_.print(text[written]);
        ++written;
    }

    for (uint8_t i = written; i < cols_; ++i)
    {
        lcd_.print(' ');
    }
}
