#include "WebUIManager.h"

WebUIManager::WebUIManager(
    BME280Processor   &processorRef,
    BME280Preferences &prefsRef,
    SignalKBroker     &signalkRef,
    DisplayManager    &displayRef
) : server(80),
    processor(processorRef),
    prefs(prefsRef),
    signalk(signalkRef),
    display(displayRef) {}

void WebUIManager::begin() {
    
}

void WebUIManager::handleRequest() {
    
}
