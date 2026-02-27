#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Using Adafruit Unified Sensor 1.1.15
// Using Adafruit BusIO 1.17.4
// Using Adafruit BME280 Library 2.3.0

static constexpr uint8_t SDA_PIN = 21;
static constexpr uint8_t SCL_PIN = 22;
static constexpr uint8_t I2C_ADDR = 0x77;

Adafruit_BME280 bme;

void setup() {

  Serial.begin(115200);
  delay(2000);
  Serial.println();
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);
  
  if (!bme.begin(I2C_ADDR)) {
    Serial.println("BME280 sensor not found!");
    while (1) delay(2000);
  }

  Serial.println("BME280 sensor connected.");

}

void loop() {

  Serial.print("TEMP: ");
  Serial.print(bme.readTemperature());
  Serial.println(" °C");

  Serial.print("HUMI: ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.print("PRES: ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  delay(3000);

}
