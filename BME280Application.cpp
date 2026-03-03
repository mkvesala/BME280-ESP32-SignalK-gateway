#include "BME280Application.h"
#include "secrets.h"

// === P U B L I C ===

BME280Application::BME280Application()
    : bme(),
      processor(bme),
      prefs(processor),
      signalk(processor),
      espnow(processor),
      display(processor, signalk),
      webui(processor, prefs, signalk, display) {}

void BME280Application::begin() {
    // 1. I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(47);

    // 2. LCD
    display.begin();

    // 3. Sensor and processor
    sensor_ok = processor.begin(Wire, BME280_ADDR);

    // 4. NVS
    prefs.load();

    // 5. Stop bluetooth
    btStop();

    // 6. WiFi AP+STA mode (ESP-NOW co-existence requirement)
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    wifi_state = WifiState::CONNECTING;
    wifi_conn_start_ms = millis();

    // 7. ESP-NOW
    espnow.begin();
}

void BME280Application::loop() {
    const unsigned long now = millis();
    handleWifi(now);
    handleOTA();
    handleWebUI();
    handleWebsocket(now);
    handleSensorRead(now);
    handleSignalK(now);
    handleESPNow(now);
    handleDisplay();
}

// === P R I V A T E ===

void BME280Application::handleWifi(unsigned long now) {
    if ((long)(now - wifi_last_check_ms) < (long)WIFI_STATUS_CHECK_MS) return;
    wifi_last_check_ms = now;

    switch (wifi_state) {
        case WifiState::INIT:
            break;

        case WifiState::CONNECTING: {
            wl_status_t status = WiFi.status();
            if (status == WL_CONNECTED) {
                wifi_state = WifiState::CONNECTED;
                initWifiServices();
                expn_retry_ms = WS_RETRY_MS;
            } else if ((long)(now - wifi_conn_start_ms) >= (long)WIFI_TIMEOUT_MS) {
                wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
            } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
                wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
            }
            break;
        }

        case WifiState::CONNECTED:
            if (!WiFi.isConnected()) {
                wifi_state = WifiState::CONNECTING;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
                wifi_conn_start_ms = now;
            }
            break;

        case WifiState::FAILED:
        case WifiState::DISCONNECTED:
        case WifiState::OFF:
            break;
    }
}

void BME280Application::initWifiServices() {
    signalk.begin();

    ArduinoOTA.setHostname(signalk.getSignalKSource());
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([]() {});
    ArduinoOTA.onEnd([]() {});
    ArduinoOTA.onError([](ota_error_t e) { (void)e; });
    ArduinoOTA.begin();

    webui.begin();
}

void BME280Application::handleOTA() {
    if (wifi_state != WifiState::CONNECTED) return;
    ArduinoOTA.handle();
}

void BME280Application::handleWebUI() {
    if (wifi_state != WifiState::CONNECTED) return;
    webui.handleRequest();
}

void BME280Application::handleWebsocket(unsigned long now) {
    if (wifi_state != WifiState::CONNECTED) return;
    signalk.handleStatus();

    if (!signalk.isOpen() && (long)(now - next_ws_try_ms) >= 0) {
        signalk.connectWebsocket();
        next_ws_try_ms = now + expn_retry_ms;
        expn_retry_ms = min(expn_retry_ms * 2, WS_RETRY_MAX_MS);
    }
    if (signalk.isOpen()) expn_retry_ms = WS_RETRY_MS;
}

void BME280Application::handleSensorRead(unsigned long now) {
    if ((long)(now - last_read_ms) < (long)READ_MS) return;
    last_read_ms = now;
    processor.update();
}

void BME280Application::handleSignalK(unsigned long now) {
    if (wifi_state != WifiState::CONNECTED) return;
    if ((long)(now - last_signalk_tx_ms) < (long)SIGNALK_TX_MS) return;
    last_signalk_tx_ms = now;
    signalk.sendDelta();
}

void BME280Application::handleESPNow(unsigned long now) {
    espnow.processIncomingCommands();
    if ((long)(now - last_espnow_tx_ms) < (long)ESPNOW_TX_MS) return;
    last_espnow_tx_ms = now;
    espnow.sendDelta();
}

void BME280Application::handleDisplay() {
    display.handle();
}
