# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-03-19

### Added

- **BME280Processor** – reads temperature (°C), relative humidity (%) and pressure (hPa) from Adafruit BME280 over I2C; exposes data via `getDelta()` as an `ESPNow::WeatherDelta` struct; validates all readings with `validf()` before publishing
- **SignalKBroker** – maintains a WebSocket connection to a SignalK server and sends delta updates every ~2 s to the paths `environment.outside.temperature` (K), `environment.outside.relativeHumidity` (ratio 0–1) and `environment.outside.pressure` (Pa); source name auto-derived from the device MAC address (`esp32.bme280-XXYYZZ`); optional token-based authentication via `secrets.h`
- **ESPNowBroker** – broadcasts `ESPNow::WeatherDelta` packets over ESP-NOW every ~2 s using the shared `espnow_protocol.h` wire format (magic `ESNW`, message type `WEATHER_DELTA`); operates on a broadcast MAC so any receiver on the same channel can consume the data without pairing
- **DisplayManager** – optional 16×2 I2C LCD (address 0x27) with I2C auto-detection at startup; shows live temperature, humidity and pressure; initialises gracefully when no display is present
- **WebUIManager** – skeleton HTTP server class following the design pattern; foundation for a future configuration web UI
- **BME280Preferences** – skeleton NVS persistence class following the design pattern; foundation for future user-adjustable settings such as temperature offset
- **BME280Application** – central orchestrator owning all subsystems as stack-allocated members; runs a non-blocking cooperative loop with individual millisecond-timer-gated handlers for sensor reads, SignalK transmits, ESP-NOW transmits, display updates, OTA and WebUI
- **WiFi state machine** – states `INIT → CONNECTING → CONNECTED` with 90-second connection timeout and fallback to `OFF` on failure or missing SSID; auto-reconnect on dropped connection
- **WebSocket exponential back-off** – reconnect interval starts at ~2 s and doubles on each failed attempt up to a ceiling of ~120 s; resets to the initial interval when the connection is restored
- **ArduinoOTA** – over-the-air firmware updates available immediately after WiFi connects; hostname set to the SignalK source name
- **ESP-NOW / WiFi co-existence** – device boots in `WIFI_AP_STA` mode with Bluetooth disabled (`btStop()`) to satisfy the ESP-NOW channel-lock requirement
- **Deadband filtering** – configurable per-value change thresholds in `SignalKBroker` and `ESPNowBroker` (all set to 0.0 in this release, meaning every new reading is forwarded)
- **`espnow_protocol.h`** – shared protocol header defining the packed `ESPNowHeader`, `ESPNowPacket<T>` template wrapper and all payload structs (`WeatherDelta`, `BatteryDelta`, `HeadingDelta`, `LevelCommand`, `LevelResponse`) used across the ESP32 gateway fleet
- **`helpers.h`** – `validf()` float validator (`!isnan && isfinite`)
- **`version.h`** – single-source firmware version string (`v1.0.0`)
- **`secrets.example.h`** – template for WiFi credentials, SignalK host/port/token and OTA password
- **`WifiState.h`** – scoped enum for WiFi state machine states

[1.0.0]: https://github.com/mkvesala/BME280-ESP32-SignalK-gateway/releases/tag/v1.0.0
