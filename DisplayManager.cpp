#include "DisplayManager.h"
#include "helpers.h"
#include <Wire.h>

DisplayManager::DisplayManager(BME280Processor &processorRef, SignalKBroker &signalkRef)
    : lcd(LCD_ADDR, LCD_COLS, LCD_ROWS),
      processor(processorRef),
      signalk(signalkRef) {}

void DisplayManager::begin() {
    // I2C probe
    Wire.beginTransmission(LCD_ADDR);
    lcd_found = (Wire.endTransmission() == 0);
    if (!lcd_found) return;

    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BME280 Gateway");
    lcd.setCursor(0, 1);
    lcd.print("Connecting...");
}

void DisplayManager::handle() {
    if (!lcd_found) return;

    auto d = processor.getDelta();
    if (!validf(d.temperature_c) || !validf(d.humidity_p) || !validf(d.pressure_hpa)) return;

    // Rivi 0: "T:23.5C  H:65.2%"
    char row0[17];
    snprintf(row0, sizeof(row0), "T:%-5.1fC H:%-4.1f%%",
             d.temperature_c, d.humidity_p);

    // Rivi 1: "P: 1013.2 hPa   "
    char row1[17];
    snprintf(row1, sizeof(row1), "P:%-7.1f hPa  ",
             d.pressure_hpa);

    lcd.setCursor(0, 0);
    lcd.print(row0);
    lcd.setCursor(0, 1);
    lcd.print(row1);
}
