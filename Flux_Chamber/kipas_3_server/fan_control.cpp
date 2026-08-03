#include "fan_control.h"
#include "config.h"

int speedPWM = 0;

void initFanHardware() {
  ledcAttach(pinFanPWM, freq, resolution);
  ledcWrite(pinFanPWM, 0); // Pastikan kipas mati saat booting
  Serial.println("[SUCCESS] Driver PWM Kipas GPIO 33 Siap.");
}

void executeFanCommand(int speed) {
  speedPWM = speed;
  
  // Clamping register 8-Bit (0 - 255)
  if (speedPWM < 0) speedPWM = 0;
  if (speedPWM > 255) speedPWM = 255;

  ledcWrite(pinFanPWM, speedPWM);
  Serial.printf("[HARDWARE] Sinyal PWM GPIO 33 di-set ke: %d\n", speedPWM);
}