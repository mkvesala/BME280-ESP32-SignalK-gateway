#include "SignalKBroker.h"
#include "helpers.h"
#include "secrets.h"

using namespace websockets;

SignalKBroker::SignalKBroker(BME280Processor &processorRef)
    : processor(processorRef) {}

bool SignalKBroker::begin() {
    if (strlen(SK_HOST) == 0 || SK_PORT == 0) return false;
    this->setSignalKURL();
    this->setSignalKSource();
    return this->connectWebsocket();
}

void SignalKBroker::handleStatus() {
    if (ws_open) ws.poll();
}

bool SignalKBroker::connectWebsocket() {
    ws_open = ws.connect(SK_URL);
    if (ws_open) {
        ws.onMessage([this](WebsocketsMessage msg) {
            this->onMessageCallback(msg);
        });
        ws.onEvent([this](WebsocketsEvent event, const String &data) {
            (void)data;
            this->onEventCallback(event);
        });
    }
    return ws_open;
}

void SignalKBroker::closeWebsocket() {
    ws.close();
    ws_open = false;
}

void SignalKBroker::setSignalKURL() {
    if (strlen(SK_TOKEN) > 0)
        snprintf(SK_URL, sizeof(SK_URL),
            "ws://%s:%d/signalk/v1/stream?token=%s", SK_HOST, SK_PORT, SK_TOKEN);
    else
        snprintf(SK_URL, sizeof(SK_URL),
            "ws://%s:%d/signalk/v1/stream", SK_HOST, SK_PORT);
}

void SignalKBroker::setSignalKSource() {
    uint8_t m[6];
    esp_efuse_mac_get_default(m);
    snprintf(SK_SOURCE, sizeof(SK_SOURCE), "esp32.bme280-%02x%02x%02x", m[3], m[4], m[5]);
}

void SignalKBroker::onEventCallback(WebsocketsEvent event) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            ws_open = true;
            break;
        case WebsocketsEvent::ConnectionClosed:
            ws_open = false;
            break;
        case WebsocketsEvent::GotPing:
            ws.pong();
            break;
        default:
            break;
    }
}

void SignalKBroker::onMessageCallback(WebsocketsMessage msg) {
    if (!msg.isText()) return;
    incoming_doc.clear();
    deserializeJson(incoming_doc, msg.data());
}

void SignalKBroker::sendDelta() {
    if (!ws_open) return;

    auto d = processor.getDelta();
    if (!validf(d.temperature_c) || !validf(d.humidity_p) || !validf(d.pressure_hpa)) return;

    // Deadband: lähetä vain jos jokin arvo on muuttunut riittävästi
    bool changed = false;
    if (!validf(last_temp_c)       || fabsf(d.temperature_c       - last_temp_c)       >= DB_TEMP_C)   changed = true;
    if (!validf(last_humidity)     || fabsf(d.humidity_p     - last_humidity)     >= DB_HUMIDITY) changed = true;
    if (!validf(last_pressure_hpa) || fabsf(d.pressure_hpa - last_pressure_hpa) >= DB_PRES_HPA) changed = true;
    if (!changed) return;

    last_temp_c       = d.temperature_c;
    last_humidity     = d.humidity_p;
    last_pressure_hpa = d.pressure_hpa;

    // SI-muunnokset SignalK:ta varten
    float temp_k       = d.temperature_c + 273.15f;
    float humidity_r   = d.humidity_p / 100.0f;
    float pressure_pa  = d.pressure_hpa * 100.0f;

    // Rakenna SignalK delta JSON
    delta_doc.clear();
    delta_doc["context"] = "vessels.self";
    auto updates = delta_doc.createNestedArray("updates");
    auto up      = updates.createNestedObject();
    up["$source"] = SK_SOURCE;
    auto values  = up.createNestedArray("values");

    auto add = [&](const char* path, float v) {
        auto o = values.createNestedObject();
        o["path"]  = path;
        o["value"] = v;
    };

    add("environment.outside.temperature", temp_k);
    add("environment.outside.humidity",    humidity_r);
    add("environment.outside.pressure",    pressure_pa);

    char buf[640];
    size_t n = serializeJson(delta_doc, buf, sizeof(buf));
    bool ok = ws.send(buf, n);
    if (!ok) {
        ws.close();
        ws_open = false;
    }
}
