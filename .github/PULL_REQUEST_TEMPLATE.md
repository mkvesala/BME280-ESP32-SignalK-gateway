## Description

Please include a summary of the change and which issue is fixed.

Fixes # (issue)

## Type of change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update

## How Has This Been Tested?

Please describe the tests that you ran to verify your changes.

- [ ] Tested on actual hardware
- [ ] ESP32 boots without errors
- [ ] BME280 sensor reads temperature, humidity and pressure correctly
- [ ] Invalid readings (NaN, infinite) are discarded and not forwarded
- [ ] LCD displays live readings (if connected); boots normally without LCD
- [ ] WiFi connects and reports a valid IP address
- [ ] SignalK WebSocket connection established, server lists ESP32 source
- [ ] SignalK paths update in Data Browser with correct units (K, ratio, Pa)
- [ ] WebSocket reconnects automatically after server restart or network drop
- [ ] ESP-NOW broadcasts arrive at the receiver device with correct values
- [ ] OTA update succeeds
- [ ] No memory leaks observed (Serial monitor)
- [ ] Task runtimes and stack watermarks stay in reasonable limits (Serial monitor)

**Test Configuration**:
- Arduino IDE: [e.g. 2.3.8]
- ESP32 board package: [e.g. 3.3.7]
- Library versions:
  - Adafruit BME280 Library: [e.g. 2.2.4]
  - ArduinoWebsockets: [e.g. 0.5.4]
  - ArduinoJson: [e.g. 7.4.2]
  - LiquidCrystal_I2C: [e.g. 1.1.2]

## Checklist:

- [ ] My code follows the style guidelines of this project
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have tested on actual hardware if possible
- [ ] I have acknowledged any AI assistance in code comments/PR description

## AI Assistance

- [ ] No AI tools used
- [ ] ChatGPT used for code generation/review
- [ ] Claude used for code generation/review
- [ ] GitHub Copilot used
- [ ] Other AI tools (specify): _____________

If AI was used, describe the prompts and how you verified the code:
```
(describe here)
```
