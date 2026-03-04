#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "BME280Processor.h"

// === C L A S S  B M E 2 8 0 P R E F E R E N C E S ===
//
// - Class BME280Preferences - responsible for NVS persistence
// - Currently obsolete skeleton

class BME280Preferences {

public:
    
    explicit BME280Preferences(BME280Processor &processorRef);

    void load();
    void saveTempOffset(float value);

private:
    
    const char* _ns = "bme280";  // NVS namespace
    
    Preferences _prefs;
    BME280Processor &_processor;

};
