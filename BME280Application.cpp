#include "BME280Application.h"
#include "secrets.h"

// === P U B L I C ===

// Constructor
BME280Application::BME280Application()
    : _bme(),
      _processor(_bme),
      _prefs(_processor),
      _signalk(_processor),
      _espnow(_processor),
      _display(_processor, _signalk),
      _webui(_processor, _prefs, _signalk, _display) {}

// Initialize
void BME280Application::begin() {
    // I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(47);

    // LCD
    _display.begin();

    // Sensor and processor
    _sensor_ok = _processor.begin(Wire, BME280_ADDR);

    // NVS
    _prefs.load();

    // Stop bluetooth
    btStop();

    // WiFi AP+STA mode (ESP-NOW co-existence requirement).
    // softAP() secures the AP interface immediately after mode() — before any
    // client can connect. Hidden SSID, WPA2 password, max 1 connection.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS, 1 /*channel*/, 1 /*ssid_hidden*/, 1 /*max_connection*/);

    // 3rd line of AP defence — register before WiFi.begin() so no event is missed.
    // Callback runs in FreeRTOS "arduino_events" task: deauth immediately, flag loop().
    // MAC is copied before setting the flag so loop() always reads a complete address.
    WiFi.onEvent([this](arduino_event_id_t /*id*/, arduino_event_info_t info) {
        uint8_t aid = info.wifi_ap_staconnected.aid;
        memcpy(_ap_intruder_mac, info.wifi_ap_staconnected.mac, 6);
        esp_wifi_deauth_sta(aid);
        _ap_intruder = true;
    }, ARDUINO_EVENT_WIFI_AP_STACONNECTED);

    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    _wifi_state = WifiState::CONNECTING;
    _wifi_conn_start_ms = millis();

    // ESP-NOW
    _espnow.begin();
}

// Loop - called in main loop()
void BME280Application::loop() {
    const unsigned long now = millis();
    this->handleWifi(now);
    this->handleAPIntruder();
    this->handleOTA();
    this->handleWebUI();
    this->handleWebsocket(now);
    this->handleSensorRead(now);
    this->handleSignalK(now);
    this->handleESPNow(now);
    this->handleDisplay();
}

// === P R I V A T E ===

// AP intruder alert — deauth done in event callback; log + display here
void BME280Application::handleAPIntruder() {
    if (!_ap_intruder) return;
    _ap_intruder = false;  // clear before Serial/display so a rapid second event isn't lost
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             _ap_intruder_mac[0], _ap_intruder_mac[1], _ap_intruder_mac[2],
             _ap_intruder_mac[3], _ap_intruder_mac[4], _ap_intruder_mac[5]);
    Serial.printf("[AP] INTRUDER deauthed — MAC %s\n", mac);
    _display.showInfoMessage("AP: INTRUDER!", mac);
}

// WiFi handler for the loop()
void BME280Application::handleWifi(unsigned long now) {
    if ((long)(now - _wifi_last_check_ms) < (long)WIFI_STATUS_CHECK_MS) return;
    _wifi_last_check_ms = now;

    switch (_wifi_state) {
        case WifiState::INIT:
            break;

        case WifiState::CONNECTING: {
            wl_status_t status = WiFi.status();
            if (status == WL_CONNECTED) {
                _wifi_state = WifiState::CONNECTED;
                this->initWifiServices();
                _expn_retry_ms = WS_RETRY_MS;
            } else if ((long)(now - _wifi_conn_start_ms) >= (long)WIFI_TIMEOUT_MS) {
                _wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
            } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
                _wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
            }
            break;
        }

        case WifiState::CONNECTED:
            if (!WiFi.isConnected()) {
                _wifi_state = WifiState::CONNECTING;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
                _wifi_conn_start_ms = now;
            }
            break;

        case WifiState::FAILED:
        case WifiState::DISCONNECTED:
        case WifiState::OFF:
            break;
    }
}

// Init WiFi-dependent stuff
void BME280Application::initWifiServices() {
    // SignalK websocket
    _signalk.begin();

    // OTA
    ArduinoOTA.setHostname(_signalk.getSignalKSource());
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([]() {});
    ArduinoOTA.onEnd([]() {});
    ArduinoOTA.onError([](ota_error_t e) { (void)e; });
    ArduinoOTA.begin();

    // WebServer (not implemented in this version)
    _webui.begin();
    
    Serial.println();
    Serial.println(WiFi.macAddress());
    Serial.println(WiFi.localIP());
}

// OTA handler for the loop()
void BME280Application::handleOTA() {
    if (_wifi_state != WifiState::CONNECTED) return;
    ArduinoOTA.handle();
}

// WebServer handler for the loop()
void BME280Application::handleWebUI() {
    if (_wifi_state != WifiState::CONNECTED) return;
    _webui.handleRequest();
}

// WebSocket handler for the loop()
void BME280Application::handleWebsocket(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;
    _signalk.handleStatus();

    if (!_signalk.isOpen() && (long)(now - _next_ws_try_ms) >= 0) {
        _signalk.connectWebsocket();
        _next_ws_try_ms = now + _expn_retry_ms;
        _expn_retry_ms = min(_expn_retry_ms * 2, WS_RETRY_MAX_MS);
    }
    if (_signalk.isOpen()) _expn_retry_ms = WS_RETRY_MS;
}

// Sensor reader for the loop()
void BME280Application::handleSensorRead(unsigned long now) {
    if ((long)(now - _last_read_ms) < (long)READ_MS) return;
    _last_read_ms = now;
    _processor.update();
}

// SignalK send handler for the loop()
void BME280Application::handleSignalK(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;
    if ((long)(now - _last_signalk_tx_ms) < (long)SIGNALK_TX_MS) return;
    _last_signalk_tx_ms = now;
    _signalk.sendDelta();
}

// ESP-NOW handler for the loop()
void BME280Application::handleESPNow(unsigned long now) {
    _espnow.processIncomingCommands();
    if ((long)(now - _last_espnow_tx_ms) < (long)ESPNOW_TX_MS) return;
    _last_espnow_tx_ms = now;
    _espnow.sendDelta();
}

// LCD handler for the loop()
void BME280Application::handleDisplay() {
    _display.handle();
}
