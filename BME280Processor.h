#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "espnow_protocol.h"

class BME280Processor {

public:
    
    explicit BME280Processor(Adafruit_BME280 &bmeRef);

    bool begin(TwoWire &wirePort, uint8_t addr);
    bool update();

    ESPNow::WeatherDelta getDelta() const { return delta; }

    float getTempC()       const { return delta.temperature_c; }
    float getHumidity()    const { return delta.humidity_p; }
    float getPressureHPa() const { return delta.pressure_hpa; }

    void setTempOffset(float v) { temp_offset = v; }
    float getTempOffset() const { return temp_offset; }

private:
    
    Adafruit_BME280 &bme;

    ESPNow::WeatherDelta delta = { NAN, NAN, NAN };
    float temp_offset = 0.0f;

};
