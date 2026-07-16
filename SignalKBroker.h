#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include <memory>
#include "BME280Processor.h"

// === C L A S S  S I G N A L K B R O K E R ===
//
// - Class SignalKBroker - responsible for SignalK server communication
// - Init: _signalk.begin();
// - Provides public API to
//  - Keep websocket alive (poll)
//  - Connect and close websocket
//  - Get connection status and source name
//  - Send BME280 data to SignalK paths
// - Uses: BME280Processor
// - Owns: WebsocketsClient (recreated per reconnect — see connectWebsocket)
// - Owned by: BME280Application

class SignalKBroker {

public:

    explicit SignalKBroker(BME280Processor &processorRef);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();

    bool isOpen() const { return _ws_open; }
    const char* getSignalKSource() const { return SK_SOURCE; }

    void ping();
    bool isStale(unsigned long now) const;

    void sendDelta();

private:

    void setSignalKURL();
    void setSignalKSource();
    void onMessageCallback(websockets::WebsocketsMessage msg);
    void onEventCallback(websockets::WebsocketsEvent event);

    BME280Processor &_processor;
    std::unique_ptr<websockets::WebsocketsClient> _ws;  // fresh instance every connect

    StaticJsonDocument<512> _delta_doc;
    StaticJsonDocument<1024> _incoming_doc;

    bool _ws_open = false;
    char SK_URL[512];
    char SK_SOURCE[32];

    // Liveness — half-open TCP detection via client ping / server pong
    static constexpr unsigned long PONG_TIMEOUT_MS = 29989UL;  // ~30 s w/o pong → stale
    unsigned long _last_pong_ms = 0;   // millis() of last GotPong / open; 0 = not connected

    // Deadband (optional)
    static constexpr float DB_TEMP_C   = 0.0f;   // °C
    static constexpr float DB_HUMIDITY = 0.0f;   // %
    static constexpr float DB_PRES_HPA = 0.0f;   // hPa

    float _last_temp_c       = NAN;
    float _last_humidity     = NAN;
    float _last_pressure_hpa = NAN;
    
};
