#pragma once
#include <Arduino.h>

// =========================================================================
// KONFIGURASI PERANGKAT KERAS
// =========================================================================
#define PIN_SENSOR_SUHU 4

// =========================================================================
// DEKLARASI VARIABEL GLOBAL (extern)
// =========================================================================
extern float suhu_air_saat_ini;
extern unsigned long interval_sampling;
extern unsigned long timer_sensor;