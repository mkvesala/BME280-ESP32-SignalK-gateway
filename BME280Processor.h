#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "espnow_protocol.h"

// === C L A S S  B M E 2 8 0 P R O C E S S O R ===
//
// - Class BME280Processor - responsible of processing the values from the sensor
// - Init: _processor.begin(..)
// - Provides public API for:
//  - Read the sensor and update data struct
//  - Get data struct and individual data points within the struct
//  - Uses: Adafruit_BME280
//  - Owned by: BME280Application

class BME280Processor {

public:
    
    explicit BME280Processor(Adafruit_BME280 &bmeRef);

    bool begin(TwoWire &wirePort, uint8_t addr);
    bool update();

    ESPNow::WeatherDelta getDelta() const { return _delta; }

    float getTempC() const { return _delta.temperature_c; }
    float getHumidity() const { return _delta.humidity_p; }
    float getPressureHPa() const { return _delta.pressure_hpa; }

private:
    
    Adafruit_BME280 &_bme;

    ESPNow::WeatherDelta _delta = { NAN, NAN, NAN };

};
