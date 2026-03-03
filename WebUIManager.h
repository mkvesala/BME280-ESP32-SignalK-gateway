#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "BME280Processor.h"
#include "BME280Preferences.h"
#include "SignalKBroker.h"
#include "DisplayManager.h"

// Empty skeleton
class WebUIManager {

public:

    explicit WebUIManager(
        BME280Processor   &processorRef,
        BME280Preferences &prefsRef,
        SignalKBroker     &signalkRef,
        DisplayManager    &displayRef
    );

    void begin();
    void handleRequest();

private:

    WebServer server;
    BME280Processor   &processor;
    BME280Preferences &prefs;
    SignalKBroker     &signalk;
    DisplayManager    &display;
    
};
