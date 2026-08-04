#include "sensor_temp.h"
#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Instansiasi objek komunikasi 1-Wire dan sensor Suhu
OneWire oneWire(PIN_SENSOR_SUHU);
DallasTemperature sensorSuhu(&oneWire);

void initSensorSuhu() {
  Serial.println("[INIT] Menginisialisasi Sensor Suhu Air (DS18B20)...");
  sensorSuhu.begin();
  
  // Validasi apakah sensor terbaca di jalur 1-Wire
  if (sensorSuhu.getDeviceCount() == 0) {
    Serial.println("[ERROR] Sensor Suhu tidak terdeteksi di GPIO 4. Cek kabel!");
  } else {
    Serial.print("[INIT] Sensor Suhu Siap. Jumlah terdeteksi: ");
    Serial.println(sensorSuhu.getDeviceCount());
  }
}

void bacaSensorSuhu() {
  // Perintahkan sensor untuk mengambil data suhu terbaru
  sensorSuhu.requestTemperatures(); 
  float suhu_baca = sensorSuhu.getTempCByIndex(0);
  
  // Validasi pembacaan (DS18B20 mengembalikan -127 jika gagal/terputus)
  if (suhu_baca == DEVICE_DISCONNECTED_C) {
    Serial.println("[ERROR] Gagal membaca suhu air!");
  } else {
    suhu_air_saat_ini = suhu_baca;
    Serial.printf("[SAMPLING] Suhu Air: %.2f °C\n", suhu_air_saat_ini);
  }
}