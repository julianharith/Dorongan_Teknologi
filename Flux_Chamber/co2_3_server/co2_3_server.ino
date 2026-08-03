#include "config.h"
#include "sensor_data.h"
#include "communication.h"

// =========================================================================
// DEFINISI VARIABEL GLOBAL (Di-instansiasi dari extern)
// =========================================================================
float current_co2          = 0.0;
float current_pressure_hpa = 1013.25; 
float current_temp_c       = 25.0;          
float fluks_co2_akhir      = 0.0;

int status_valve_vakum     = -1; 
int status_kipas_pwm       = -1;   
bool modeOtomatis          = true; 
unsigned long intervalSampling = 1000; 
bool responValveDiterima   = false; 

State currentState = STATE_INIT_MEASURE;
int subStateFlushing = 0;         
unsigned long timerPublishLive = 0; 
bool cmdTutupTerkirim = false; 
unsigned long timerState = 0;
unsigned long waktuMulaiSampling = 0;
int jumlahSampel = 0;
float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0; 
float dCdt_sekarang = 0.0, dCdt_sebelumnya = 999.0, dCdt_maksimal = 0.0; 

// =========================================================================
// FUNGSI UTAMA
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("========================================");
  Serial.println("Node Utama CO2 Master State Machine");
  Serial.println("========================================");

  setupCommunication();
  initSensorHardware();
}

void loop() {
  // 1. Pemeliharaan Jaringan & Komunikasi
  maintainCommunication();

  // 2. Pembacaan Sensor Berkelanjutan
  readSensorData();
  
  // 3. Telemetri Realtime
  if (currentState == STATE_SAMPLING && (millis() - timerPublishLive >= 2000)) {
    timerPublishLive = millis();
    String strCO2  = String(current_co2, 0);
    String strDCDT = String(dCdt_sekarang, 4);

    publishJSONToVPSAndLocal(TOPIC_PUB_CO2, "Modul_CO2", "co2", "ppm", strCO2, "normal", modeOtomatis ? "Auto Sampling Active" : "Manual Monitoring Active");
    publishJSONToVPSAndLocal(TOPIC_PUB_DCDT, "Modul_dCdt", "dcdt", "ppm/s", strDCDT, "normal", "Live Gradient");
  }

  // 4. Finite State Machine (FSM) Engine
  switch (currentState) {
    
    case STATE_INIT_MEASURE:
      checkAndConnectCloudServices();

      if (!modeOtomatis) { 
        cmdTutupTerkirim = false;
        waktuMulaiSampling = millis();
        timerState = millis();
        dCdt_sekarang = 0.0;
        currentState = STATE_SAMPLING;
        break;
      }

      if (!cmdTutupTerkirim) {
        Serial.println("\n[MASTER] >>> Membuka Siklus Baru: Memerintahkan Valve Menutup... <<<");
        responValveDiterima = false; 
        status_valve_vakum = -1; 

        // String untuk Lokal
        String valvePayloadLocal = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Valve_Vaccum\",\"label_macAddr\":\"68:fe:71:87:47:c4\",\"data_kontrol\":\"1\",\"data_status\":\"OK\",\"data_berita\":\"Perintah Tutup Katup\"}";
        String fanPayloadLocal   = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Fan\",\"label_macAddr\":\"68:fe:71:13:1d:14\",\"data_kontrol\":\"100\",\"data_status\":\"OK\",\"data_berita\":\"Set Kipas Homogen\"}";
        
        // String untuk VPS
        String valvePayloadVPS = "{\"sensorNode_farmda27b11b_68:fe:71:87:47:c4\":[{\"label_lokasi\":\"farmda27b11b\",\"label_nama_Modul\":\"sensor_co2\",\"label_variabel\":\"valve\",\"label_satuan\":\"state\",\"label_macAddr\":\"68:fe:71:87:47:c4\",\"data_kontrol\":1,\"data_status\":\"normal\",\"data_berita\":\"Perintah Tutup Katup\"}]}";
        String fanPayloadVPS   = "{\"sensorNode_farmda27b11b_68:fe:71:13:1d:14\":[{\"label_lokasi\":\"farmda27b11b\",\"label_nama_Modul\":\"sensor_co2\",\"label_variabel\":\"fan_pwm\",\"label_satuan\":\"%\",\"label_macAddr\":\"68:fe:71:13:1d:14\",\"data_kontrol\":100,\"data_status\":\"normal\",\"data_berita\":\"Set Kipas Homogen\"}]}";
        
        if (clientLocal.connected()) {
          clientLocal.publish(TOPIC_PUB_CMD_VALVE, valvePayloadLocal.c_str());
          clientLocal.publish(TOPIC_PUB_CMD_FAN, fanPayloadLocal.c_str());
        }
        if (clientVPS.connected()) {
          // Topik diubah menjadi farmda27b11b saja
          clientVPS.publish("farmda27b11b", valvePayloadVPS.c_str());
          clientVPS.publish("farmda27b11b", fanPayloadVPS.c_str());
          
          clientVPS.publish(TOPIC_PUB_CMD_VALVE, valvePayloadVPS.c_str());
          clientVPS.publish(TOPIC_PUB_CMD_FAN, fanPayloadVPS.c_str());
        }
        cmdTutupTerkirim = true; 
      }

      if (responValveDiterima && status_valve_vakum == 0) {
        Serial.println("[MASTER] Handshake Sukses! Katup Terkonfirmasi Rapat.");
        Serial.println("[MASTER] Mengunci Waktu Dasar T0 & Memulai Perhitungan Grafik Laju...\n");

        sumX = 0; sumY = 0; sumXY = 0; sumX2 = 0;
        jumlahSampel = 0;
        dCdt_sekarang = 0.0;
        dCdt_sebelumnya = 999.0; 
        dCdt_maksimal = 0.0; 

        waktuMulaiSampling = millis();
        timerState = millis();
        
        cmdTutupTerkirim = false; 
        currentState = STATE_SAMPLING; 
      }
      break;

    case STATE_SAMPLING:
      if (millis() - timerState >= intervalSampling) { 
        timerState = millis();
        float t_detik = (millis() - waktuMulaiSampling) / 1000.0; 
        jumlahSampel++;

        sumX  += t_detik;
        sumY  += current_co2;
        sumXY += (t_detik * current_co2);
        sumX2 += (t_detik * t_detik);

        if (jumlahSampel > 2) {
          float penyebut = (jumlahSampel * sumX2) - (sumX * sumX);
          if (penyebut != 0) {
            dCdt_sekarang = ((jumlahSampel * sumXY) - (sumX * sumY)) / penyebut;
          }
          
          if (t_detik >= 10.0 && dCdt_sekarang > 0 && dCdt_sekarang > dCdt_maksimal) {
            dCdt_maksimal = dCdt_sekarang; 
          }

          Serial.printf("[SAMPLING] t: %.1fs | CO2: %.1f | dC/dt: %.4f | dC/dt Max: %.4f | Mode: %s\n", 
                        t_detik, current_co2, dCdt_sekarang, dCdt_maksimal, modeOtomatis ? "AUTO" : "MANUAL");

          if (modeOtomatis) {
            if (t_detik > 20.0 && (dCdt_sebelumnya - dCdt_sekarang) > 0 && dCdt_sekarang < THRESHOLD_SATURASI) {
              Serial.println("[MASTER] >>> Kurva Laju Mendatar/Saturasi! Menghentikan Pengukuran <<<");
              currentState = STATE_CALC_AND_FLUSH;
              subStateFlushing = 0;
            }
            dCdt_sebelumnya = dCdt_sekarang; 
          }
        }
        
        if (modeOtomatis && t_detik >= 180.0) {
          Serial.println("[MASTER] >>> Timeout Batas Maksimal 3 Menit Tercapai <<<");
          currentState = STATE_CALC_AND_FLUSH;
          subStateFlushing = 0;
        }
      }
      break;

    case STATE_CALC_AND_FLUSH:
      if (subStateFlushing == 0) {
        float i_suhu_kelvin    = current_temp_c + 273.15;           
        float i_tekanan_pascal = current_pressure_hpa * 100.0;   

        float rasio_dimensi = VOLUME / AREA;
        float kerapatan_gas = i_tekanan_pascal / (R_GAS * i_suhu_kelvin);
        fluks_co2_akhir     = rasio_dimensi * kerapatan_gas * dCdt_maksimal;

        String strFlux = String(fluks_co2_akhir, 6);

        publishJSONToVPSAndLocal(TOPIC_PUB_FLUX, "Modul_Flux_Result", "flux", "mg/m2/h", strFlux, "normal", "Nilai Fluks Maksimal Berhasil Dikunci");
        
        if (modeOtomatis) {
          // String untuk Lokal
          String fanOffPayloadLocal    = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Fan\",\"label_macAddr\":\"68:fe:71:13:1d:14\",\"data_kontrol\":\"0\",\"data_status\":\"OK\",\"data_berita\":\"Kipas Dimatikan Pra-Flushing\"}";
          String valveOpenPayloadLocal = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Valve_Vaccum\",\"label_macAddr\":\"68:fe:71:87:47:c4\",\"data_kontrol\":\"0\",\"data_status\":\"OK\",\"data_berita\":\"Perintah Buka Katup & Flush\"}";
          
          // String untuk VPS
          String fanOffPayloadVPS    = "{\"sensorNode_farmda27b11b_68:fe:71:13:1d:14\":[{\"label_lokasi\":\"farmda27b11b\",\"label_nama_Modul\":\"sensor_co2\",\"label_variabel\":\"fan_pwm\",\"label_satuan\":\"%\",\"label_macAddr\":\"68:fe:71:13:1d:14\",\"data_kontrol\":0,\"data_status\":\"normal\",\"data_berita\":\"Kipas Dimatikan Pra-Flushing\"}]}";
          String valveOpenPayloadVPS = "{\"sensorNode_farmda27b11b_68:fe:71:87:47:c4\":[{\"label_lokasi\":\"farmda27b11b\",\"label_nama_Modul\":\"sensor_co2\",\"label_variabel\":\"valve\",\"label_satuan\":\"state\",\"label_macAddr\":\"68:fe:71:87:47:c4\",\"data_kontrol\":0,\"data_status\":\"normal\",\"data_berita\":\"Perintah Buka Katup & Flush\"}]}";

          if (clientLocal.connected()) {
            clientLocal.publish(TOPIC_PUB_CMD_FAN, fanOffPayloadLocal.c_str());
            clientLocal.publish(TOPIC_PUB_CMD_VALVE, valveOpenPayloadLocal.c_str());
          }
          if (clientVPS.connected()) {
            // Topik diubah menjadi farmda27b11b saja
            clientVPS.publish("farmda27b11b", fanOffPayloadVPS.c_str());
            clientVPS.publish("farmda27b11b", valveOpenPayloadVPS.c_str());
            
            clientVPS.publish(TOPIC_PUB_CMD_FAN, fanOffPayloadVPS.c_str());
            clientVPS.publish(TOPIC_PUB_CMD_VALVE, valveOpenPayloadVPS.c_str());
          }
        }
        
        responValveDiterima = false; 
        status_valve_vakum  = -1;
        timerState          = millis(); 
        subStateFlushing    = 1; 
      }
      
      else if (subStateFlushing == 1) {
        if (!modeOtomatis) {
          timerState = millis();
          subStateFlushing = 2;
          break;
        }

        if (responValveDiterima && status_valve_vakum == 1) {
          Serial.println("[MASTER] Handshake Sukses! Katup Resmi Terbuka Lebar di Lapangan.");
          timerState = millis(); 
          subStateFlushing = 2;
        }
        else if (millis() - timerState >= 12000) { 
          Serial.println("\n[⚠️ CRITICAL TIMEOUT] Paket Handshake Valve Hilang.");
          responValveDiterima = true;
          status_valve_vakum  = 1;
          timerState          = millis(); 
          subStateFlushing    = 2;
        }
      }
      
      else if (subStateFlushing == 2) {
        if (!modeOtomatis) {
          subStateFlushing = 0;
          currentState = STATE_INIT_MEASURE;
          break;
        }

        if (millis() - timerState >= 30000) { 
          Serial.println("[MASTER] Fase Vakum Dan Flushing Selesai.");
          delay(2000); 
          subStateFlushing = 0;
          
          dCdt_sekarang   = 0.0;
          dCdt_sebelumnya = 999.0;
          dCdt_maksimal   = 0.0;
          jumlahSampel    = 0;
          
          currentState = STATE_INIT_MEASURE; 
          break;
        }
      }
      break;
  }
}