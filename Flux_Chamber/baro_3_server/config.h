#pragma once
#include <Arduino.h>

// =========================================================================
// 1. PINOUT BUS I2C CUSTOM
// =========================================================================
#define PIN_SDA 21
#define PIN_SCL 22

// =========================================================================
// 2. KREDENSIAL BROKER MQTT (VPS & LOCAL)
// =========================================================================
const char* const VPS_HOST      = "hivemqtt.e-farmingcorpora.cloud";
const int         VPS_PORT      = 8883;
const char* const VPS_USER      = "efarming";
const char* const VPS_PASS      = "EfarmingHiveMQ2026!";
const char* const VPS_CLIENT_ID = "sensorbaro_node_vps";

const char* const LOCAL_HOST      = "192.168.0.130"; 
const int         LOCAL_PORT      = 1883;
const char* const LOCAL_USER      = "efarming-local";
const char* const LOCAL_PASS      = "12345abc";
const char* const LOCAL_CLIENT_ID = "sensorbaro_node_local";

// Metadata Perangkat
const char* const LOKASI_ALAT = "Workshop";
const char* const MAC_ADDRESS = "68:fe:71:13:02:d8";

// =========================================================================
// 3. ROUTING TOPIK MQTT
// =========================================================================
const char* const TOPIC_VPS_PUB_TEMP       = "edufarm/flux/sensor/temperature"; 
const char* const TOPIC_VPS_PUB_BARO       = "edufarm/flux/sensor/baro";        
const char* const TOPIC_VPS_SUB_INTERVAL   = "edufarm/flux/sensor/baro/interval"; 

const char* const TOPIC_LOCAL_PUB_TEMP     = "edufarm/flux/sensor/temperature"; 
const char* const TOPIC_LOCAL_PUB_BARO     = "edufarm/flux/sensor/baro";        
const char* const TOPIC_LOCAL_SUB_INTERVAL = "edufarm/flux/sensor/baro/interval"; 

// =========================================================================
// 4. DEKLARASI VARIABEL GLOBAL (EXTERN)
// =========================================================================
extern float tekanan_hPa;
extern float suhu_c;
extern unsigned long intervalPublishBaro;