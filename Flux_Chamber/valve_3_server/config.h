#pragma once
#include <Arduino.h>

// =========================================================================
// 1. PINOUT DRIVER RELAY PERANGKAT KERAS
// =========================================================================
const int pinBuka   = 26; 
const int pinTutup  = 25; 
const int pinVacuum = 33; 

const char* const LOKASI_ALAT = "Workshop";
const char* const NAMA_MODUL  = "Modul_Actuator_Valve_Vaccum";
const char* const MAC_ADDRESS = "68:fe:71:87:47:c4"; 

// =========================================================================
// 2. KREDENSIAL BROKER MQTT (VPS & LOCAL)
// =========================================================================
const char* const VPS_HOST      = "hivemqtt.e-farmingcorpora.cloud";
const int         VPS_PORT      = 8883;
const char* const VPS_USER      = "efarming";
const char* const VPS_PASS      = "EfarmingHiveMQ2026!";
const char* const VPS_CLIENT_ID = "sensorvalve_node_vps";

const char* const LOCAL_HOST      = "192.168.0.130"; 
const int         LOCAL_PORT      = 1883;
const char* const LOCAL_USER      = "efarming-local";
const char* const LOCAL_PASS      = "12345abc";
const char* const LOCAL_CLIENT_ID = "sensorvalve_node_local";

// =========================================================================
// 3. ROUTING TOPIK MQTT
// =========================================================================
const char* const TOPIC_VPS_CMD_VALVE = "edufarm/flux/actuator/valve"; 
const char* const TOPIC_VPS_STS_VALVE = "edufarm/flux/valve/status";   

const char* const TOPIC_LOCAL_CMD_VALVE = "edufarm/flux/actuator/valve"; 
const char* const TOPIC_LOCAL_STS_VALVE = "edufarm/flux/valve/status";   

// =========================================================================
// 4. DEKLARASI VARIABEL GLOBAL (EXTERN)
// =========================================================================
// Tidak ada variabel global spesifik yang perlu diekspor untuk katup.