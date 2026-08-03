#pragma once
#include <Arduino.h>

// =========================================================================
// 1. PINOUT & PARAMETER PULSE WIDTH MODULATION (PWM)
// =========================================================================
const int pinFanPWM  = 33;   
const int freq       = 5000; 
const int resolution = 8;    

const char* const LOKASI_ALAT = "Workshop";
const char* const NAMA_MODUL  = "Modul_Actuator_Fan";
const char* const MAC_ADDRESS = "68:fe:71:13:1d:14"; 

// =========================================================================
// 2. KREDENSIAL BROKER MQTT (VPS & LOCAL)
// =========================================================================
const char* const VPS_HOST      = "hivemqtt.e-farmingcorpora.cloud";
const int         VPS_PORT      = 8883;
const char* const VPS_USER      = "efarming";
const char* const VPS_PASS      = "EfarmingHiveMQ2026!";
const char* const VPS_CLIENT_ID = "sensorfan_node_vps";

const char* const LOCAL_HOST      = "192.168.0.130"; 
const int         LOCAL_PORT      = 1883;
const char* const LOCAL_USER      = "efarming-local";
const char* const LOCAL_PASS      = "12345abc";
const char* const LOCAL_CLIENT_ID = "sensorfan_node_local";

// =========================================================================
// 3. ROUTING TOPIK MQTT
// =========================================================================
const char* const TOPIC_VPS_CMD_FAN = "edufarm/flux/actuator/fan"; 
const char* const TOPIC_VPS_STS_FAN = "edufarm/flux/fan/status";   

const char* const TOPIC_LOCAL_CMD_FAN = "edufarm/flux/actuator/fan"; 
const char* const TOPIC_LOCAL_STS_FAN = "edufarm/flux/fan/status";   

// =========================================================================
// 4. DEKLARASI VARIABEL GLOBAL (EXTERN)
// =========================================================================
extern int speedPWM;