# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.1] - 2026-07-29

### Changed

- **Shared `espnow_protocol.h` updated to the fleet-wide superset** - the hand-copied protocol header had drifted into four diverging versions across the boat's ESP32 projects; they were merged into a single superset and every project now holds an identical copy. Adds the `HALMET_WATER_DELTA` (7) and `DEPTH_DELTA` (8) message types with their payload structs (`HALMETWaterDelta`, `DepthDelta`), and reserves `GENERIC_SK_DELTA` (20) for a possible future runtime-configurable path relay (documented only, not implemented). The header now also documents the ownership rules — copy the whole file to every project, never reconcile two versions field by field — and the fleet-wide `msg_type` allocation table listing every sender and receiver. No existing enum value or payload struct was modified, so the wire format is unchanged and every existing receiver stays compatible; this gateway's own firmware logic and the `WeatherDelta` it transmits are untouched

### Fixed

- **Position and SOG hidden at anchor on ESP-NOW receivers** - the receiver-side helper `convertGnssDeltaToData()` in the shared header folded a NaN COG into `fix_ok`, so a stationary boat — which has a valid position fix but an undefined course — appeared to have no fix at all and the display suppressed position and SOG as well. `GnssData` now tracks position validity (`fix_ok`) and course validity (`cog_valid`) separately, exposed as `hasFix()` / `hasCog()`, and carries `lat_deg` / `lon_deg` for the receiver's position display. Receiver-side conversion only — this gateway compiles the struct but never uses it, and the data it transmits is unaffected

## [1.2.0] - 2026-07-16

### Fixed

- **Permanent WebSocket reconnect failure** - the gateway could lose its SignalK WebSocket connection after ~12–48 h of continuous operation and never recover until a manual reboot, even though WiFi stayed connected, heap and stack were healthy, the main loop kept running, and stale detection plus exponential backoff fired correctly; `SignalKBroker` reused a single lifetime `WebsocketsClient` (and thus the same underlying `WiFiClient` / lwIP socket) across every reconnect, so once that socket entered a stuck state every subsequent `connect()` reused the corrupted transport and failed forever. `connectWebsocket()` now constructs a brand-new `WebsocketsClient` on every attempt (registering callbacks before `connect()`) and destroys it immediately on a failed connect, guaranteeing each reconnect starts from a clean TCP / WebSocket state; the lwIP socket fd is released by the client destructor

### Changed

- **`SignalKBroker` owns the WebSocket client via `std::unique_ptr`** - the client is created per connect and destroyed on teardown rather than kept as a permanent value member; `closeWebsocket()` now destroys the object (previously it only called `close()` and left it for reuse), all `_ws` operations (`poll()`, `ping()`, `pong()`, `send()`) are pointer-guarded, and the send-failure path in `sendDelta()` routes through `closeWebsocket()` so client destruction happens in exactly one place. The reconnect, exponential backoff, ping/pong liveness and SignalK protocol handling in `BME280Application::handleWebsocket()` are unchanged

## [1.1.0] - 2026-04-24

### Added

- **WiFi AP security — hidden SSID** - `WiFi.softAP()` called immediately after `WiFi.mode(WIFI_AP_STA)` with `ssid_hidden=1` so the AP interface is never advertised; the AP is required for ESP-NOW / WiFi co-existence but is not intended for external clients
- **WiFi AP security — WPA2 password** - AP protected with a WPA2 passphrase (`AP_PASS`, minimum 8 characters) configured in `secrets.h`; `secrets.example.h` updated with the new `AP_SSID` and `AP_PASS` constants
- **WiFi AP intrusion detection** - `WiFi.onEvent(ARDUINO_EVENT_WIFI_AP_STACONNECTED)` registered in `begin()` before `WiFi.begin()` so no connection event is missed; the FreeRTOS callback copies the intruder MAC atomically and calls `esp_wifi_deauth_sta()` to kick the client immediately, then sets a `volatile bool` flag for `loop()`
- **`handleAPIntruder()`** - lightweight `loop()`-context handler that clears the intrusion flag, formats the MAC address and writes a `[AP] INTRUDER deauthed` log line to Serial and a two-line alert to the LCD via `DisplayManager::showInfoMessage()`
- **`DisplayManager::showInfoMessage()`** - new public method that clears the LCD and displays an arbitrary two-line message; used by the intrusion-detection path and available for other alert use cases
- **`#include <esp_wifi.h>`** added to `BME280Application.h` to expose `esp_wifi_deauth_sta()`
- **WebSocket liveness — active ping/pong** - `SignalKBroker::ping()` sends a client-initiated WebSocket ping frame and the `GotPong` event refreshes `_last_pong_ms`; `BME280Application::handleWebsocket()` pings every `WS_PING_MS` (~10 s) while the socket is open
- **Half-open TCP detection** - `SignalKBroker::isStale()` reports a connection where `isOpen()` still returns `true` but no pong has arrived within `PONG_TIMEOUT_MS` (~30 s); `handleWebsocket()` then calls `closeWebsocket()` and lets the existing exponential backoff reconnect, recovering from a silently-dead SignalK server (e.g. macOS host power-saving) without dropping WiFi

### Fixed

- **WiFi reconnect resource leak** - `initWifiServices()` now guards itself with a `_wifi_services_initialized` flag so `ArduinoOTA.begin()` (UDP socket + mDNS) and `WebUIManager::begin()` (HTTP route registration) are called only once per boot; previously each WiFi reconnection re-invoked both, silently leaking a UDP socket and WebServer handler entries until the heap was exhausted

### Changed

- **`WIFI_TIMEOUT_MS`** increased from 90 001 ms (~90 s) to 179 999 ms (~3 min) to allow more time for WiFi association in congested or distant network environments
- **Liveness recovery is transport-only** - the silently-dead connection is now healed by a graceful WebSocket reconnect instead of an `ESP.restart()`; this supersedes the reboot watchdog added in the earlier (reverted) change, which keyed off `isOpen()` and could not detect a half-open TCP while wrongly rebooting a healthy device during transient server outages

## [1.0.1] - 2026-04-06

### Changed

Patching documentation only. Updated README with references to the updated UML class diagrams (master now in ESP32-Crowpanel-compass repo).

## [1.0.0] - 2026-03-19

### Added

- **BME280Processor** - reads temperature (°C), relative humidity (%) and pressure (hPa) from Adafruit BME280 over I2C; exposes data via `getDelta()` as an `ESPNow::WeatherDelta` struct; validates all readings with `validf()` before publishing
- **SignalKBroker** - maintains a WebSocket connection to a SignalK server and sends delta updates every ~2 s to the paths `environment.outside.temperature` (K), `environment.outside.relativeHumidity` (ratio 0-1) and `environment.outside.pressure` (Pa); source name auto-derived from the device MAC address (`esp32.bme280-XXYYZZ`); optional token-based authentication via `secrets.h`
- **ESPNowBroker** - broadcasts `ESPNow::WeatherDelta` packets over ESP-NOW every ~2 s using the shared `espnow_protocol.h` wire format (magic `ESNW`, message type `WEATHER_DELTA`); operates on a broadcast MAC so any receiver on the same channel can consume the data without pairing
- **DisplayManager** - optional 16x2 I2C LCD (address 0x27) with I2C auto-detection at startup; shows live temperature, humidity and pressure; initialises gracefully when no display is present
- **WebUIManager** - skeleton HTTP server class following the design pattern; foundation for a future configuration web UI
- **BME280Preferences** - skeleton NVS persistence class following the design pattern; foundation for future user-adjustable settings such as temperature offset
- **BME280Application** - central orchestrator owning all subsystems as stack-allocated members; runs a non-blocking cooperative loop with individual millisecond-timer-gated handlers for sensor reads, SignalK transmits, ESP-NOW transmits, display updates, OTA and WebUI
- **WiFi state machine** - states `INIT → CONNECTING → CONNECTED` with 90-second connection timeout and fallback to `OFF` on failure or missing SSID; auto-reconnect on dropped connection
- **WebSocket exponential back-off** - reconnect interval starts at ~2 s and doubles on each failed attempt up to a ceiling of ~120 s; resets to the initial interval when the connection is restored
- **ArduinoOTA** - over-the-air firmware updates available immediately after WiFi connects; hostname set to the SignalK source name
- **ESP-NOW / WiFi co-existence** - device boots in `WIFI_AP_STA` mode with Bluetooth disabled (`btStop()`) to satisfy the ESP-NOW channel-lock requirement
- **Deadband filtering** - configurable per-value change thresholds in `SignalKBroker` and `ESPNowBroker` (all set to 0.0 in this release, meaning every new reading is forwarded)
- **`espnow_protocol.h`** - shared protocol header defining the packed `ESPNowHeader`, `ESPNowPacket<T>` template wrapper and all payload structs (`WeatherDelta`, `BatteryDelta`, `HeadingDelta`, `LevelCommand`, `LevelResponse`) used across the ESP32 gateway fleet
- **`helpers.h`** - `validf()` float validator (`!isnan && isfinite`)
- **`version.h`** - single-source firmware version string (`v1.0.0`)
- **`secrets.example.h`** - template for WiFi credentials, SignalK host/port/token and OTA password
- **`WifiState.h`** - scoped enum for WiFi state machine states

[1.2.1]: https://github.com/mkvesala/BME280-ESP32-SignalK-gateway/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/mkvesala/BME280-ESP32-SignalK-gateway/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/mkvesala/BME280-ESP32-SignalK-gateway/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/mkvesala/BME280-ESP32-SignalK-gateway/releases/tag/v1.0.1
[1.0.0]: https://github.com/mkvesala/BME280-ESP32-SignalK-gateway/releases/tag/v1.0.0
