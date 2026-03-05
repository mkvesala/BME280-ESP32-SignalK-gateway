#include "SignalKBroker.h"
#include "helpers.h"
#include "secrets.h"

using namespace websockets;

// === P U B L I C ===

// Constructor
SignalKBroker::SignalKBroker(BME280Processor &processorRef)
    : _processor(processorRef) {}

// Initialize
bool SignalKBroker::begin() {
    if (strlen(SK_HOST) == 0 || SK_PORT == 0) return false;
    this->setSignalKURL();
    this->setSignalKSource();
    return this->connectWebsocket();
}

// Maintain websocket connection
void SignalKBroker::handleStatus() {
    if (_ws_open) _ws.poll();
}

// Connect websocket
bool SignalKBroker::connectWebsocket() {
    _ws_open = _ws.connect(SK_URL);
    if (_ws_open) {
        _ws.onMessage([this](WebsocketsMessage msg) {
            this->onMessageCallback(msg);
        });
        _ws.onEvent([this](WebsocketsEvent event, const String &data) {
            (void)data;
            this->onEventCallback(event);
        });
    }
    return _ws_open;
}

// Close websocket
void SignalKBroker::closeWebsocket() {
    _ws.close();
    _ws_open = false;
}

// Send BME280 data to SignalK paths
void SignalKBroker::sendDelta() {
    if (!_ws_open) return;

    auto d = _processor.getDelta();
    if (!validf(d.temperature_c) || !validf(d.humidity_p) || !validf(d.pressure_hpa)) return;

    // Deadband
    bool changed = false;
    if (!validf(_last_temp_c) || fabsf(d.temperature_c - _last_temp_c) >= DB_TEMP_C) changed = true;
    if (!validf(_last_humidity) || fabsf(d.humidity_p - _last_humidity) >= DB_HUMIDITY) changed = true;
    if (!validf(_last_pressure_hpa) || fabsf(d.pressure_hpa - _last_pressure_hpa) >= DB_PRES_HPA) changed = true;
    if (!changed) return;

    _last_temp_c = d.temperature_c;
    _last_humidity = d.humidity_p;
    _last_pressure_hpa = d.pressure_hpa;

    // SI units for SignalK
    float temp_k = d.temperature_c + 273.15f;    // kelvin
    float humidity_r = d.humidity_p / 100.0f;    // ratio
    float pressure_pa = d.pressure_hpa * 100.0f; // pascal

    // Build SignalK delta
    _delta_doc.clear();
    _delta_doc["context"] = "vessels.self";
    auto updates = _delta_doc.createNestedArray("updates");
    auto up = updates.createNestedObject();
    up["$source"] = SK_SOURCE;
    auto values = up.createNestedArray("values");

    auto add = [&](const char* path, float v) {
        auto o = values.createNestedObject();
        o["path"]  = path;
        o["value"] = v;
    };

    add("environment.outside.temperature", temp_k);
    add("environment.outside.relativeHumidity", humidity_r);
    add("environment.outside.pressure", pressure_pa);

    char buf[640];
    size_t n = serializeJson(_delta_doc, buf, sizeof(buf));
    bool ok = _ws.send(buf, n);
    if (!ok) {
        _ws.close();
        _ws_open = false;
    }
}

// === P R I V A T E ===

// Create SignalK URL for websocket
void SignalKBroker::setSignalKURL() {
    if (strlen(SK_TOKEN) > 0)
        snprintf(SK_URL, sizeof(SK_URL),
            "ws://%s:%d/signalk/v1/stream?token=%s", SK_HOST, SK_PORT, SK_TOKEN);
    else
        snprintf(SK_URL, sizeof(SK_URL),
            "ws://%s:%d/signalk/v1/stream", SK_HOST, SK_PORT);
}

// Set source name for SignalK
void SignalKBroker::setSignalKSource() {
    uint8_t m[6];
    esp_efuse_mac_get_default(m);
    snprintf(SK_SOURCE, sizeof(SK_SOURCE), "esp32.bme280-%02x%02x%02x", m[3], m[4], m[5]);
}

// Websocket callback
void SignalKBroker::onEventCallback(WebsocketsEvent event) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            _ws_open = true;
            break;
        case WebsocketsEvent::ConnectionClosed:
            _ws_open = false;
            break;
        case WebsocketsEvent::GotPing:
            _ws.pong();
            break;
        default:
            break;
    }
}

// Websocket callback
void SignalKBroker::onMessageCallback(WebsocketsMessage msg) {
    if (!msg.isText()) return;
    _incoming_doc.clear();
    deserializeJson(_incoming_doc, msg.data());
}