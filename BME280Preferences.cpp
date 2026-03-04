#include "BME280Preferences.h"

// === P U B L I C ===

// Constructor
BME280Preferences::BME280Preferences(BME280Processor &processorRef)
    : _processor(processorRef) {}

// Load from NVS
void BME280Preferences::load() {
    if (!_prefs.begin(_ns, true)) return;
    _processor.setTempOffset(_prefs.getFloat("temp_offset", 0.0f));
    _prefs.end();
}

// Save something to NVS
void BME280Preferences::saveTempOffset(float value) {
    (void)value;
}
