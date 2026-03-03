#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "BME280Processor.h"

class BME280Preferences {

public:
    
    explicit BME280Preferences(BME280Processor &processorRef);

    void load();
    void saveTempOffset(float value);

private:
    
    const char* ns = "bme280";  // NVS namespace
    
    Preferences prefs;
    BME280Processor &processor;

};
