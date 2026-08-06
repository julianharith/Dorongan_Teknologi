#include <Arduino.h>
#include "config.h"
#include "sensor_ultrasonic.h"
#include "communication.h"

// Variabel global
float jarak_cm = 0.0;
unsigned long interval_ultrasonik = 1000; 
unsigned long timer_ultrasonik = 0;

// =========================================================================
// FUNGSI SETUP UTAMA
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("========================================");
  Serial.println("Node Sensor Jarak (Ultrasonik & MQTT)");
  Serial.println("========================================");

  setupCommunication();
  initSensorUltrasonik();
}

// =========================================================================
// FUNGSI LOOP UTAMA (CORE 1) - Bebas Hambatan (Non-Blocking)
// =========================================================================
void loop() {
  // 1. Pemeliharaan Jaringan
  maintainCommunication();

  // 2. Penjadwalan Pengukuran Sensor
  if (millis() - timer_ultrasonik >= interval_ultrasonik) {
    timer_ultrasonik = millis();
    
    // Eksekusi pembacaan
    bacaSensorUltrasonik();
    
    // 3. Validasi & Pengiriman
    if (jarak_cm > 0.0 && jarak_cm <= 600.0) {
      String strJarak = String(jarak_cm, 2);
      
      publishJarakData(strJarak);
    }
  }
}