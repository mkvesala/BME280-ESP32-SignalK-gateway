#include "ESPNowBroker.h"
#include "helpers.h"

// == P U B L I C ===

// Constructor
ESPNowBroker::ESPNowBroker(BME280Processor &processorRef)
    : _processor(processorRef) {}

// Initialize
bool ESPNowBroker::begin() {
    if (esp_now_init() != ESP_OK) return false;

    // Register broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    // Register callbacks
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    _initialized = true;
    return _initialized;
}

// Send BME280 data as ESP-NOW broadcast
void ESPNowBroker::sendDelta() {
    if (!_initialized) return;

    auto d = _processor.getDelta();
    if (!validf(d.temperature_c) || !validf(d.humidity_p) || !validf(d.pressure_hpa)) return;

    // Deadband
    bool changed = false;
    if (!validf(_last_temp_c) || fabsf(d.temperature_c - _last_temp_c) >= DB_TEMP_C) changed = true;
    if (!validf(_last_humidity) || fabsf(d.humidity_p - _last_humidity) >= DB_HUMIDITY) changed = true;
    if (!validf(_last_pressure_hpa) || fabsf(d.pressure_hpa - _last_pressure_hpa) >= DB_PRES_HPA) changed = true;
    if (!changed) return;

    _last_temp_c       = d.temperature_c;
    _last_humidity     = d.humidity_p;
    _last_pressure_hpa = d.pressure_hpa;

    // Build ESPNowPacket: header + WeatherDelta payload
    ESPNow::ESPNowPacket<ESPNow::WeatherDelta> pkt;
    ESPNow::initHeader(pkt.hdr, ESPNow::ESPNowMsgType::WEATHER_DELTA, sizeof(ESPNow::WeatherDelta));
    memcpy(&pkt.payload, &d, sizeof(ESPNow::WeatherDelta));

    esp_now_send(BROADCAST_ADDR, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

// Process inbound commands
void ESPNowBroker::processIncomingCommands() {
    // Not implemented in this version
    if (!_command_received) return;
    _command_received = false;
}

// Static callback
void ESPNowBroker::onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status) {
    (void)info;
    (void)status;
}

// Static callback
void ESPNowBroker::onDataRecv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    if (len < (int)sizeof(ESPNow::ESPNowHeader)) return;

    ESPNow::ESPNowHeader hdr;
    memcpy(&hdr, data, sizeof(ESPNow::ESPNowHeader));
    if (hdr.magic != ESPNow::ESPNOW_MAGIC) return;
    if (len < (int)(sizeof(ESPNow::ESPNowHeader) + hdr.payload_len)) return;

    (void)recv_info;
}
