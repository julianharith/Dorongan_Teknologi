#include <Arduino.h>
#include "config.h"
#include "sensor_temp.h"

// =========================================================================
// INSTANSIASI VARIABEL GLOBAL
// =========================================================================
float suhu_air_saat_ini = 0.0;
unsigned long interval_sampling = 2000; // Pembacaan setiap 2 detik
unsigned long timer_sensor = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("========================================");
  Serial.println("Node Sensor Suhu Air Mulai Beroperasi");
  Serial.println("========================================");

  // Panggil fungsi inisialisasi dari modul sensor
  initSensorSuhu();
}

void loop() {
  // Mekanisme FSM sederhana berbasis waktu untuk sampling
  if (millis() - timer_sensor >= interval_sampling) {
    timer_sensor = millis();
    bacaSensorSuhu();
  }
}