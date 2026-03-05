#include "BME280Processor.h"
#include "helpers.h"

// === P U B L I C ===

// Constructor
BME280Processor::BME280Processor(Adafruit_BME280 &bmeRef)
    : _bme(bmeRef) {}

// Initialize
bool BME280Processor::begin(TwoWire &wirePort, uint8_t addr) {
    return _bme.begin(addr, &wirePort);
}

// Read values from sensor and update the data struct
bool BME280Processor::update() {
    float t = _bme.readTemperature();
    float h = _bme.readHumidity();
    float p = _bme.readPressure() / 100.0f;  // Pa -> hPa

    if (!validf(t) || !validf(h) || !validf(p)) return false;

    _delta.temperature_c = t;
    _delta.humidity_p = h;
    _delta.pressure_hpa = p;

    return true;
}
