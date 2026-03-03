#include <Arduino.h>
#include "BME280Application.h"

// === M A I N  P R O G R A M ===
//
// - Owns BME280Application instance
// - Initiates serial
// - Initiates application, halts if critical failure
// - Executes app.loop() in the main loop()

BME280Application app;

// Setup
void setup() {
  Serial.begin(115200);
  delay(47);

  app.begin();

  if (!app.sensorOk()) {
    Serial.println("BME280 INIT FAILED! CHECK WIRING AND I2C ADDRESS.");
    while (1) delay(1999);
  }
}

// Main loop
void loop() {
  app.loop();
}
