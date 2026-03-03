#include "BME280Processor.h"
#include "helpers.h"

BME280Processor::BME280Processor(Adafruit_BME280 &bmeRef)
    : bme(bmeRef) {}

bool BME280Processor::begin(TwoWire &wirePort, uint8_t addr) {
    return bme.begin(addr, &wirePort);
}

bool BME280Processor::update() {
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0f;  // Pa -> hPa

    if (!validf(t) || !validf(h) || !validf(p)) return false;

    delta.temperature_c       = t + temp_offset;
    delta.humidity_p     = h;
    delta.pressure_hpa = p;

    return true;
}
