#include <Arduino.h>
#include "config.h"
#include "sensor_ultrasonic.h"
#include "communication.h"

float jarak_cm = 0.0;
unsigned long interval_ultrasonik = 1000; 
unsigned long timer_ultrasonik = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("========================================");
  Serial.println("Node Sensor Jarak (Ultrasonik & MQTT)");
  Serial.println("========================================");

  setupCommunication();
  initSensorUltrasonik();
}

void loop() {
  maintainCommunication();

  if (millis() - timer_ultrasonik >= interval_ultrasonik) {
    timer_ultrasonik = millis();
    bacaSensorUltrasonik();
    
    if (jarak_cm > 0.0 && jarak_cm <= 600.0) {
      String strJarak = String(jarak_cm, 2);
      
      // Publikasi dikembalikan menggunakan 1 topik saja
      publishJSONToVPSAndLocal(
        TOPIC_PUB_JARAK, 
        "Modul_Ultrasonic", 
        "jarak", 
        "cm", 
        strJarak, 
        "normal", 
        "Data Jarak Permukaan Aktual"
      );
    }
  }
}