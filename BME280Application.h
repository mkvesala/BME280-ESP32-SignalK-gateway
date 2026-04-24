#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>
#include <Adafruit_BME280.h>
#include "WifiState.h"
#include "BME280Processor.h"
#include "BME280Preferences.h"
#include "SignalKBroker.h"
#include "ESPNowBroker.h"
#include "DisplayManager.h"
#include "WebUIManager.h"

// === C L A S S  B M E 2 8 0 A P P L I C A T I O N ===
//
// - Class BME280Application - responsible for orchestrating everything
// - Init: app.begin();
// - Loop: app.loop();
// - Critical guard: app.sensorOk();
// - Owns: Adafruit_BME280, BME280Processor, BME280Preferences, SignalKBroker, ESPNowBroker, DisplayManager, WebUIManager

class BME280Application {

public:

    explicit BME280Application();

    void begin();
    void loop();
    bool sensorOk() const { return _sensor_ok; }

private:

    // Hardware constants
    static constexpr uint8_t I2C_SDA     = 21;
    static constexpr uint8_t I2C_SCL     = 22;
    static constexpr uint8_t BME280_ADDR = 0x77;

    // Timer constants
    static constexpr unsigned long READ_MS              = 997;
    static constexpr unsigned long SIGNALK_TX_MS        = 1999;
    static constexpr unsigned long ESPNOW_TX_MS         = 2003;
    static constexpr unsigned long WIFI_STATUS_CHECK_MS = 503;
    static constexpr unsigned long WIFI_TIMEOUT_MS      = 179999;
    static constexpr unsigned long WS_RETRY_MS          = 1999;
    static constexpr unsigned long WS_RETRY_MAX_MS      = 119993;

    // Timers
    unsigned long _expn_retry_ms      = WS_RETRY_MS;
    unsigned long _next_ws_try_ms     = 0;
    unsigned long _last_read_ms       = 0;
    unsigned long _last_signalk_tx_ms = 0;
    unsigned long _last_espnow_tx_ms  = 0;
    unsigned long _wifi_conn_start_ms = 0;
    unsigned long _wifi_last_check_ms = 0;

    bool _sensor_ok = false;
    WifiState _wifi_state = WifiState::INIT;

    // AP intruder detection — written in WiFi event callback, read in loop()
    volatile bool _ap_intruder      = false;
    uint8_t       _ap_intruder_mac[6] = {};

    // Stack allocated instances owned by the app
    Adafruit_BME280   _bme;
    BME280Processor   _processor;
    BME280Preferences _prefs;
    SignalKBroker     _signalk;
    ESPNowBroker      _espnow;
    DisplayManager    _display;
    WebUIManager      _webui;

    // Handlers for app.loop()
    void handleWifi(unsigned long now);
    void handleAPIntruder();
    void handleOTA();
    void handleWebUI();
    void handleWebsocket(unsigned long now);
    void handleSensorRead(unsigned long now);
    void handleSignalK(unsigned long now);
    void handleESPNow(unsigned long now);
    void handleDisplay();

    void initWifiServices();
    
};
