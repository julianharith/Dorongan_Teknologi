#pragma once
#include <Arduino.h>

// =========================================================================
// 1. KREDENSIAL MULTI-WIFI & BROKER MQTT
// =========================================================================
const char* const VPS_HOST      = "hivemqtt.e-farmingcorpora.cloud";
const int         VPS_PORT      = 8883;
const char* const VPS_USER      = "efarming";
const char* const VPS_PASS      = "EfarmingHiveMQ2026!";
const char* const VPS_CLIENT_ID = "sensorflux_master_vps";

const char* const LOCAL_HOST      = "192.168.15.85";
const int         LOCAL_PORT      = 1883;
const char* const LOCAL_CLIENT_ID = "sensorflux_master_local";

// --- CLOUD SERVER (COMMENTED) ---
// const char* const CLOUD_HOST      = "c06a31d1daa24624a3b0340899bf4fe3.s1.eu.hivemq.cloud"; 
// const int         CLOUD_PORT      = 8883; 
// const char* const CLOUD_USER      = "dortek2026_consumer_wsan";                  
// const char* const CLOUD_PASS      = "DORtek2026";                  
// const char* const CLOUD_CLIENT_ID = "sensorflux_master_cloud"; 

const char* const LOKASI_ALAT = "Workshop";
const char* const MAC_ADDRESS = "68:fe:71:12:e7:74";

// =========================================================================
// 2. KONSTANTA FISIKA & HARDWARE
// =========================================================================
const float VOLUME = 0.0036652; 
const float AREA   = 0.031416;  
const float R_GAS  = 8.3144;    
const float THRESHOLD_SATURASI = 0.05; 

#define RX_PIN 16  
#define TX_PIN 17  

// =========================================================================
// 3. ROUTING TOPIK MQTT
// =========================================================================
const char* const TOPIC_PUB_CO2        = "edufarm/flux/master/co2_realtime";
const char* const TOPIC_PUB_DCDT       = "edufarm/flux/master/dcdt_realtime"; 
const char* const TOPIC_PUB_FLUX       = "edufarm/flux/master/flux_result";
const char* const TOPIC_PUB_CMD_VALVE  = "edufarm/flux/actuator/valve";
const char* const TOPIC_PUB_CMD_FAN    = "edufarm/flux/actuator/fan";

const char* const TOPIC_SUB_BARO       = "edufarm/flux/sensor/baro";        
const char* const TOPIC_SUB_TEMP       = "edufarm/flux/sensor/temperature"; 
const char* const TOPIC_SUB_STS_VALVE  = "edufarm/flux/valve/status";       
const char* const TOPIC_SUB_STS_FAN    = "edufarm/flux/fan/status";         
const char* const TOPIC_SUB_CMD_REMOTE = "edufarm/flux/master/command";     
const char* const TOPIC_SUB_INTERVAL   = "edufarm/flux/master/interval";    
const char* const TOPIC_SUB_MODE       = "edufarm/flux/master/mode";        

// --- CLOUD TOPIC (COMMENTED) ---
// const char* const TOPIC_CLOUD_GATEWAY  = "v1/gateway/data";

// =========================================================================
// 4. DEKLARASI VARIABEL GLOBAL (EXTERN)
// =========================================================================
extern float current_co2;
extern float current_pressure_hpa;
extern float current_temp_c;
extern float fluks_co2_akhir;

extern int status_valve_vakum; 
extern int status_kipas_pwm;
extern bool modeOtomatis;
extern unsigned long intervalSampling;
extern bool responValveDiterima;

enum State { STATE_INIT_MEASURE, STATE_SAMPLING, STATE_CALC_AND_FLUSH };
extern State currentState;
extern int subStateFlushing;
extern unsigned long timerPublishLive; 
extern bool cmdTutupTerkirim; 
extern unsigned long timerState;
extern unsigned long waktuMulaiSampling;
extern int jumlahSampel;
extern float sumX, sumY, sumXY, sumX2; 
extern float dCdt_sekarang, dCdt_sebelumnya, dCdt_maksimal;