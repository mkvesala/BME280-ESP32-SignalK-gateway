#pragma once
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "BME280Processor.h"
#include "SignalKBroker.h"

class DisplayManager {

public:

    explicit DisplayManager(BME280Processor &processorRef, SignalKBroker &signalkRef);

    void begin();
    void handle();

private:
    
    static constexpr uint8_t LCD_ADDR = 0x27;
    static constexpr uint8_t LCD_COLS = 16;
    static constexpr uint8_t LCD_ROWS = 2;

    LiquidCrystal_I2C lcd;
    bool lcd_found = false;

    BME280Processor &processor;
    SignalKBroker   &signalk;
};
