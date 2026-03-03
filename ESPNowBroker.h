#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include "BME280Processor.h"
#include "espnow_protocol.h"

class ESPNowBroker {

public:

    explicit ESPNowBroker(BME280Processor &processorRef);

    bool begin();
    void sendDelta();
    void processIncomingCommands();

private:

    BME280Processor &processor;
    bool initialized = false;

    // Deadband threasholds
    static constexpr float DB_TEMP_C   = 0.1f;   // °C
    static constexpr float DB_HUMIDITY = 0.5f;   // %
    static constexpr float DB_PRES_HPA = 0.5f;   // hPa

    float last_temp_c       = NAN;
    float last_humidity     = NAN;
    float last_pressure_hpa = NAN;

    static constexpr uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Static callbacks
    static void onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status);
    static void onDataRecv(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);

    // Reception handling callback -> loop()
    static volatile bool command_received;
    static uint8_t last_sender_mac[6];
    
};
