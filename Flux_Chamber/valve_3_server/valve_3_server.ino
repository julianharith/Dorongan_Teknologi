#include "config.h"
#include "valve_control.h"
#include "communication.h"

// =========================================================================
// FUNGSI UTAMA
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("========================================");
  Serial.println("Node Eksekutor Katup & Pompa Vakum");
  Serial.println("========================================");

  initValveHardware();
  setupCommunication();
}

void loop() {
  // Hanya mengeksekusi perawatan koneksi dan polling data MQTT yang masuk
  maintainCommunication();
}