#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "BME280Processor.h"
#include "BME280Preferences.h"
#include "SignalKBroker.h"
#include "DisplayManager.h"

// === C L A S S  W E B U I M A N A G E R ===
//
// - Class WebUIManager - responsible for running the webserver for http-requests
// - Not implemented in this version
// - Init: _webui.begin();
// - Loop: _webui.handleRequest();
// - Uses: BME280Processor, BME280Preferences, SignalKBroker, DisplayManager
// - Owns: WebServer
// - Owned by: BME280Application

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

    WebServer _server;
    BME280Processor   &_processor;
    BME280Preferences &_prefs;
    SignalKBroker     &_signalk;
    DisplayManager    &_display;
    
};
