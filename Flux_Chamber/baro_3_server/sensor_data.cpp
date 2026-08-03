#include "sensor_data.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme; 

void initSensorHardware() {
  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    Serial.println("[CRITICAL] Sensor BME280 tidak terdeteksi pada bus I2C!");
    while (1) delay(10);
  }
  Serial.println("[SUCCESS] Sensor BME280 Siap Digunakan!");
}

void readSensorData() {
  tekanan_hPa = bme.readPressure() / 100.0F; 
  suhu_c      = bme.readTemperature();
}