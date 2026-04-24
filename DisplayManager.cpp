#include "DisplayManager.h"
#include "helpers.h"
#include <Wire.h>

// === P U B L I C ===

// Constructor
DisplayManager::DisplayManager(BME280Processor &processorRef, SignalKBroker &signalkRef)
    : _lcd(LCD_ADDR, LCD_COLS, LCD_ROWS),
      _processor(processorRef),
      _signalk(signalkRef) {}

// Initialize
void DisplayManager::begin() {
    // I2C probe
    Wire.beginTransmission(LCD_ADDR);
    _lcd_found = (Wire.endTransmission() == 0);
    if (!_lcd_found) return;

    _lcd.init();
    _lcd.backlight();
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print("BME280 Gateway");
    _lcd.setCursor(0, 1);
    _lcd.print("Connecting...");
}

// Show a two-line info message on the LCD (e.g. security alerts)
void DisplayManager::showInfoMessage(const char* line1, const char* line2) {
    if (!_lcd_found) return;
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print(line1);
    _lcd.setCursor(0, 1);
    _lcd.print(line2);
}

// Update display in app.loop()
void DisplayManager::handle() {
    if (!_lcd_found) return;

    auto d = _processor.getDelta();
    if (!validf(d.temperature_c) || !validf(d.humidity_p) || !validf(d.pressure_hpa)) return;

    // Row 0: "T:23.5C  H:65.2%"
    char row0[17];
    snprintf(row0, sizeof(row0), "T:%-5.1fC H:%-4.1f%%",
             d.temperature_c, d.humidity_p);

    // Row 1: "P: 1013.2 hPa   "
    char row1[17];
    snprintf(row1, sizeof(row1), "P:%-7.1f hPa  ",
             d.pressure_hpa);

    _lcd.setCursor(0, 0);
    _lcd.print(row0);
    _lcd.setCursor(0, 1);
    _lcd.print(row1);
}
