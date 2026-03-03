#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Adafruit_BME280.h>
#include "WifiState.h"
#include "BME280Processor.h"
#include "BME280Preferences.h"
#include "SignalKBroker.h"
#include "ESPNowBroker.h"
#include "DisplayManager.h"
#include "WebUIManager.h"

class BME280Application {

public:

    explicit BME280Application();

    void begin();
    void loop();
    bool sensorOk() const { return sensor_ok; }

private:

    // Hardware constants
    static constexpr uint8_t I2C_SDA     = 21;
    static constexpr uint8_t I2C_SCL     = 22;
    static constexpr uint8_t BME280_ADDR = 0x77;

    // Timer constants
    static constexpr unsigned long READ_MS              = 2003;
    static constexpr unsigned long SIGNALK_TX_MS        = 1999;
    static constexpr unsigned long ESPNOW_TX_MS         = 997;
    static constexpr unsigned long WIFI_STATUS_CHECK_MS = 503;
    static constexpr unsigned long WIFI_TIMEOUT_MS      = 90001;
    static constexpr unsigned long WS_RETRY_MS          = 1999;
    static constexpr unsigned long WS_RETRY_MAX_MS      = 119993;

    // Timers
    unsigned long expn_retry_ms      = WS_RETRY_MS;
    unsigned long next_ws_try_ms     = 0;
    unsigned long last_read_ms       = 0;
    unsigned long last_signalk_tx_ms = 0;
    unsigned long last_espnow_tx_ms  = 0;
    unsigned long wifi_conn_start_ms = 0;
    unsigned long wifi_last_check_ms = 0;

    bool sensor_ok = false;
    WifiState wifi_state = WifiState::INIT;

    // Stack allocated instances owned by the app
    Adafruit_BME280   bme;
    BME280Processor   processor;
    BME280Preferences prefs;
    SignalKBroker     signalk;
    ESPNowBroker      espnow;
    DisplayManager    display;
    WebUIManager      webui;

    // Handler-metodit
    void handleWifi(unsigned long now);
    void handleOTA();
    void handleWebUI();
    void handleWebsocket(unsigned long now);
    void handleSensorRead(unsigned long now);
    void handleSignalK(unsigned long now);
    void handleESPNow(unsigned long now);
    void handleDisplay();

    void initWifiServices();
    
};
