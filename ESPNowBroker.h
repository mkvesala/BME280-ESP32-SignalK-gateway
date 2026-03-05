#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include "BME280Processor.h"
#include "espnow_protocol.h"

// === C L A S S  E S P N O W B R O K E R ===
//
// - Class ESPNowBroker - responsible for ESP-NOW communication
// - Init: _espnow.begin();
// - Provides public API to send data as broadcast and for processing inbound commands
// - Uses: BME280Processor
// - Owned by: BME280Application

class ESPNowBroker {

public:

    explicit ESPNowBroker(BME280Processor &processorRef);

    bool begin();
    void sendDelta();
    void processIncomingCommands();

private:

    BME280Processor &_processor;
    bool _initialized = false;

    // Deadband threasholds (optional)
    static constexpr float DB_TEMP_C   = 0.0f;   // °C
    static constexpr float DB_HUMIDITY = 0.0f;   // %
    static constexpr float DB_PRES_HPA = 0.0f;   // hPa

    float _last_temp_c       = NAN;
    float _last_humidity     = NAN;
    float _last_pressure_hpa = NAN;

    static constexpr uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Static callbacks
    static void onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status);
    static void onDataRecv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);

    // Static reception handling callback -> loop()
    inline static volatile bool _command_received = false;
    inline static uint8_t _last_sender_mac[6] = {0};
    
};
