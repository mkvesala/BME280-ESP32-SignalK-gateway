#include "WebUIManager.h"

// === P U B L I C ===

// Constructor
WebUIManager::WebUIManager(
    BME280Processor   &processorRef,
    BME280Preferences &prefsRef,
    SignalKBroker     &signalkRef,
    DisplayManager    &displayRef
) : _server(80),
    _processor(processorRef),
    _prefs(prefsRef),
    _signalk(signalkRef),
    _display(displayRef) {}


// Initialize
void WebUIManager::begin() {
    
}

// Handle requests (called in loop())
void WebUIManager::handleRequest() {
    
}
