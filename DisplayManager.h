#pragma once
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "BME280Processor.h"
#include "SignalKBroker.h"

// === C L A S S  D I S P L A Y M A N A G E R ===
//
// - Class DisplayManager - responsible for managing LCD or other display
// - Init: _display.begin();
// - Loop: _display.handle();
// - Uses: BME280Processor, SignalKBroker
// - Owned by: BME280Application

class DisplayManager {

public:

    explicit DisplayManager(BME280Processor &processorRef, SignalKBroker &signalkRef);

    void begin();
    void handle();
    void showInfoMessage(const char* line1, const char* line2);

private:
    
    static constexpr uint8_t LCD_ADDR = 0x27;
    static constexpr uint8_t LCD_COLS = 16;
    static constexpr uint8_t LCD_ROWS = 2;

    LiquidCrystal_I2C _lcd;
    bool _lcd_found = false;

    BME280Processor &_processor;
    SignalKBroker &_signalk;
};
