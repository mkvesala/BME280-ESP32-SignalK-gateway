#include "ESPNowBroker.h"
#include "helpers.h"

// Static
volatile bool ESPNowBroker::command_received = false;
uint8_t ESPNowBroker::last_sender_mac[6] = {0};

ESPNowBroker::ESPNowBroker(BME280Processor &processorRef)
    : processor(processorRef) {}

bool ESPNowBroker::begin() {
    if (esp_now_init() != ESP_OK) return false;

    // Rekisteröi broadcast-peer (channel=0 = käytä WiFin nykyistä kanavaa)
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) return false;

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    initialized = true;
    return true;
}

void ESPNowBroker::sendDelta() {
    if (!initialized) return;

    auto d = processor.getDelta();
    if (!validf(d.temperature_c) || !validf(d.humidity_p) || !validf(d.pressure_hpa)) return;

    // Deadband
    bool changed = false;
    if (!validf(last_temp_c)       || fabsf(d.temperature_c       - last_temp_c)       >= DB_TEMP_C)   changed = true;
    if (!validf(last_humidity)     || fabsf(d.humidity_p     - last_humidity)     >= DB_HUMIDITY) changed = true;
    if (!validf(last_pressure_hpa) || fabsf(d.pressure_hpa - last_pressure_hpa) >= DB_PRES_HPA) changed = true;
    if (!changed) return;

    last_temp_c       = d.temperature_c;
    last_humidity     = d.humidity_p;
    last_pressure_hpa = d.pressure_hpa;

    // Build ESPNowPacket: header + WeatherDelta payload
    ESPNow::ESPNowPacket<ESPNow::WeatherDelta> pkt;
    ESPNow::initHeader(pkt.hdr, ESPNow::ESPNowMsgType::WEATHER_DELTA, sizeof(ESPNow::WeatherDelta));
    memcpy(&pkt.payload, &d, sizeof(ESPNow::WeatherDelta));

    esp_now_send(BROADCAST_ADDR, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

void ESPNowBroker::processIncomingCommands() {
    // Not implemented in this version
    if (!command_received) return;
    command_received = false;
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
