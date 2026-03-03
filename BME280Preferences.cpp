#include "BME280Preferences.h"

BME280Preferences::BME280Preferences(BME280Processor &processorRef)
    : processor(processorRef) {}

void BME280Preferences::load() {
    if (!prefs.begin(ns, true)) return;
    processor.setTempOffset(prefs.getFloat("temp_offset", 0.0f));
    prefs.end();
}

void BME280Preferences::saveTempOffset(float value) {
    (void)value;
}
