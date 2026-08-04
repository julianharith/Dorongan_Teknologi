#include "sensor_ultrasonic.h"
#include "config.h"

void initSensorUltrasonik() {
  Serial.println("[INIT] Menginisialisasi Sensor Jarak AJ-SR04M...");
  
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  
  digitalWrite(PIN_TRIG, LOW);
  
  Serial.println("[INIT] Sensor Jarak Siap.");
}

void bacaSensorUltrasonik() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duration = pulseIn(PIN_ECHO, HIGH, 35000);
  
  float distanceCm = duration * 0.034 / 2.0;
  
  if (distanceCm == 0 || distanceCm > 600) {
    Serial.println("[WARNING] Ultrasonik: Di luar jangkauan (atau terlalu dekat)");
  } else {
    jarak_cm = distanceCm;
    Serial.printf("[SAMPLING] Jarak: %.2f cm\n", jarak_cm);
  }
}