#include "sensor_data.h"
#include "config.h"
#include <MHZ19.h>

MHZ19 myMHZ19;
HardwareSerial mhzSerial(2); 

void initSensorHardware() {
  mhzSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  myMHZ19.begin(mhzSerial);
  myMHZ19.autoCalibration(false); 
  myMHZ19.setRange(5000); 

  Serial.println("\n[⚙️ SENSOR WARMUP] Menjalankan isolasi pra-pemanasan MH-Z19C selama 10 detik...");
  unsigned long timerWarmup = millis();
  while (millis() - timerWarmup < 10000) {
    int read_warm = myMHZ19.getCO2();
    if (read_warm > 0) current_co2 = (float)read_warm;
    Serial.printf("[WARMUP TIME] Sisa Waktu: %lu detik | PPM Terbaca: %.0f\n", 10 - ((millis() - timerWarmup)/1000), current_co2);
    delay(2000); 
  }
  Serial.println("[⚙️ SENSOR WARMUP] Sensor stabil! Masuk ke sistem kendali utama.\n");
}

void readSensorData() {
  int ppm_baca = myMHZ19.getCO2();
  if (ppm_baca > 0) current_co2 = (float)ppm_baca;
}