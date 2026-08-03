#include "config.h"
#include "sensor_data.h"
#include "communication.h"

// =========================================================================
// DEFINISI VARIABEL GLOBAL
// =========================================================================
float tekanan_hPa = 0.0;
float suhu_c = 0.0;
unsigned long intervalPublishBaro = 2000; 
unsigned long timerPublish = 0;

// =========================================================================
// FUNGSI UTAMA
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("========================================");
  Serial.println("Node Telemetri Barometrik & Suhu (BME280)");
  Serial.println("========================================");

  setupCommunication();
  initSensorHardware();
}

void loop() {
  // 1. Pemeliharaan Jaringan & Komunikasi Non-Blocking
  maintainCommunication();

  // 2. Penjadwalan Pengiriman Data Berbasis Interval Dinamis
  if (millis() - timerPublish >= intervalPublishBaro) {
    timerPublish = millis();

    // 3. Eksekusi Pembacaan Sensor
    readSensorData();

    // 4. Validasi & Pengiriman ke Server
    if (!isnan(tekanan_hPa) && !isnan(suhu_c)) {
      publishBaroData(suhu_c, tekanan_hPa);
      Serial.println();
    } else {
      Serial.println("[ERROR] Gagal Membaca Data Dari Sensor BME280!");
    }
  }
}