#pragma once
#include <Arduino.h>

// =========================================================================
// KREDENSIAL MULTI-WIFI & BROKER MQTT
// =========================================================================
const char* const VPS_HOST      = "hivemqtt.e-farmingcorpora.cloud";
const int         VPS_PORT      = 8883;
const char* const VPS_USER      = "efarming";
const char* const VPS_PASS      = "EfarmingHiveMQ2026!";

// Kredensial Server Lokal (Dimatikan sementara di communication.cpp)
const char* const LOCAL_HOST      = "192.168.0.130";
const int         LOCAL_PORT      = 1883;
const char* const LOCAL_USER      = "efarming-local";
const char* const LOCAL_PASS      = "12345abc";
const char* const LOCAL_CLIENT_ID = "sensorjarak_master_local";

const char* const LOKASI_ALAT = "Workshop";
const char* const MAC_ADDRESS = "68:fe:71:12:e7:74"; 

// =========================================================================
// ROUTING TOPIK MQTT (DIKEMBALIKAN MENJADI 1 TOPIK)
// =========================================================================
const char* const TOPIC_PUB_JARAK = "edufarm/jarak/master/ultrasonic_realtime";

// =========================================================================
// KONFIGURASI PERANGKAT KERAS SENSOR
// =========================================================================
#define PIN_TRIG 12
#define PIN_ECHO 14

// =========================================================================
// DEKLARASI VARIABEL GLOBAL (extern)
// =========================================================================
extern float jarak_cm;
extern unsigned long interval_ultrasonik;
extern unsigned long timer_ultrasonik;