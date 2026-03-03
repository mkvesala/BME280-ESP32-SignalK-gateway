#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include "BME280Processor.h"

class SignalKBroker {

public:

    explicit SignalKBroker(BME280Processor &processorRef);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();

    bool isOpen() const { return ws_open; }
    const char* getSignalKSource() const { return SK_SOURCE; }

    void sendDelta();

private:

    void setSignalKURL();
    void setSignalKSource();
    void onMessageCallback(websockets::WebsocketsMessage msg);
    void onEventCallback(websockets::WebsocketsEvent event);

    BME280Processor &processor;
    websockets::WebsocketsClient ws;

    StaticJsonDocument<512>  delta_doc;
    StaticJsonDocument<1024> incoming_doc;

    bool ws_open = false;
    char SK_URL[512];
    char SK_SOURCE[32];

    // Deadband
    static constexpr float DB_TEMP_C   = 0.1f;   // °C
    static constexpr float DB_HUMIDITY = 0.5f;   // %
    static constexpr float DB_PRES_HPA = 0.5f;   // hPa

    float last_temp_c       = NAN;
    float last_humidity     = NAN;
    float last_pressure_hpa = NAN;
    
};
