/**
 * =========================================================================
 * Dokumen Kode Program: Node Utama CO2 Master State Machine (Multi-Broker)
 * Penulis: Ilham Muhammad Yusuf
 * Deskripsi: FSM Pengukuran Fluks Karbon dengan Offline-First (Rate-Limited DNS Check)
 * =========================================================================
 */

#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>       
#include <MHZ19.h>

// =========================================================================
// 1. KREDENSIAL MULTI-WIFI
// =========================================================================
WiFiMulti wifiMulti;

// =========================================================================
// 2. KONFIGURASI 3 BROKER MQTT
// =========================================================================
const char* VPS_HOST      = "hivemqtt.e-farmingcorpora.cloud";
const int   VPS_PORT      = 8883;
const char* VPS_USER      = "efarming";
const char* VPS_PASS      = "EfarmingHiveMQ2026!";
const char* VPS_CLIENT_ID = "sensorflux_master_vps";

const char* LOCAL_HOST      = "192.168.15.85"; // IP SERVER LOKAL
const int   LOCAL_PORT      = 1883;
const char* LOCAL_CLIENT_ID = "sensorflux_master_local";

const char* CLOUD_HOST      = "c06a31d1daa24624a3b0340899bf4fe3.s1.eu.hivemq.cloud"; 
const int   CLOUD_PORT      = 8883; 
const char* CLOUD_USER      = "dortek2026_consumer_wsan";                  
const char* CLOUD_PASS      = "DORtek2026";                  
const char* CLOUD_CLIENT_ID = "sensorflux_master_cloud"; 

const char* LOKASI_ALAT = "Workshop";
const char* MAC_ADDRESS = "68:fe:71:12:e7:74";

// =========================================================================
// 3. KONSTANTA PARAMETER FISIKA CLOSED CHAMBER
// =========================================================================
const float VOLUME = 0.0036652; 
const float AREA   = 0.031416;  
const float R_GAS  = 8.3144;    

// =========================================================================
// 4. ROUTING TOPIK MQTT
// =========================================================================
const char* TOPIC_PUB_CO2        = "edufarm/flux/master/co2_realtime";
const char* TOPIC_PUB_DCDT       = "edufarm/flux/master/dcdt_realtime"; 
const char* TOPIC_PUB_FLUX       = "edufarm/flux/master/flux_result";
const char* TOPIC_PUB_CMD_VALVE  = "edufarm/flux/actuator/valve";
const char* TOPIC_PUB_CMD_FAN    = "edufarm/flux/actuator/fan";

const char* TOPIC_SUB_BARO       = "edufarm/flux/sensor/baro";        
const char* TOPIC_SUB_TEMP       = "edufarm/flux/sensor/temperature"; 
const char* TOPIC_SUB_STS_VALVE  = "edufarm/flux/valve/status";       
const char* TOPIC_SUB_STS_FAN    = "edufarm/flux/fan/status";         
const char* TOPIC_SUB_CMD_REMOTE = "edufarm/flux/master/command";     
const char* TOPIC_SUB_INTERVAL   = "edufarm/flux/master/interval";    
const char* TOPIC_SUB_MODE       = "edufarm/flux/master/mode";        

const char* TOPIC_CLOUD_GATEWAY  = "v1/gateway/data";

// =========================================================================
// 5. REGISTRASI VARIABEL TELEMETRI JARINGAN & INTERNAL
// =========================================================================
float current_co2          = 0.0;
float current_pressure_hpa = 1013.25; 
float current_temp_c        = 25.0;          
float fluks_co2_akhir      = 0.0;

int status_valve_vakum = -1; 
int status_kipas_pwm   = -1;   

unsigned long timerPublishLive  = 0; 
bool cmdTutupTerkirim           = false; 

unsigned long intervalSampling = 1000; 
bool modeOtomatis             = true; 

// =========================================================================
// 6. HARDWARE SENSOR MH-Z19C
// =========================================================================
#define RX_PIN 16  
#define TX_PIN 17  
MHZ19 myMHZ19;
HardwareSerial mhzSerial(2); 

// =========================================================================
// 7. VARIABEL REGRESI LINEAR & FILTER BLANKING TIME
// =========================================================================
unsigned long waktuMulaiSampling = 0;
int jumlahSampel                 = 0;
float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0; 
float dCdt_sekarang   = 0.0;
float dCdt_sebelumnya = 999.0;
float dCdt_maksimal   = 0.0; 

const float THRESHOLD_SATURASI = 0.05; 

// =========================================================================
// 8. FINITE STATE MACHINE (FSM) ENGINE
// =========================================================================
enum State { STATE_INIT_MEASURE, STATE_SAMPLING, STATE_CALC_AND_FLUSH };
State currentState = STATE_INIT_MEASURE;

int subStateFlushing     = 0;         
bool responValveDiterima = false; 
unsigned long timerState = 0;

// =========================================================================
// 9. INSTANSIASI CLIENTS & TIMER
// =========================================================================
WiFiClientSecure espClientVPS;
PubSubClient clientVPS(espClientVPS);

WiFiClient espClientLocal;
PubSubClient clientLocal(espClientLocal);

WiFiClientSecure espClientCloud;
PubSubClient clientCloud(espClientCloud);

unsigned long lastLocalTryTimer  = 0;
unsigned long lastCloudCheckTimer = 0; // Timer pencegah spam DNS

// =========================================================================
// FUNGSI: MULTI-SERVER PUBLISHER UTILITY
// =========================================================================
void publishJSONToVPSAndLocal(const char* topic, const char* nama_modul, String data_kontrol, String data_status, String data_berita) {
  StaticJsonDocument<256> doc;
  doc["label_lokasi"]     = LOKASI_ALAT;
  doc["label_nama Modul"] = nama_modul;   
  doc["label_macAddr"]    = MAC_ADDRESS;  
  doc["data_kontrol"]     = data_kontrol;
  doc["data_status"]      = data_status;
  doc["data_berita"]      = data_berita;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  if (clientLocal.connected()) clientLocal.publish(topic, jsonPayload.c_str());
  if (clientVPS.connected())   clientVPS.publish(topic, jsonPayload.c_str());
}

void publishGatewayCloud(String module_name, String data_ctrl, String status_msg, String news) {
  if (clientCloud.connected()) {
    StaticJsonDocument<512> docCloud;
    String gatewayKey = module_name + "_" + String(LOKASI_ALAT) + "_" + String(MAC_ADDRESS);
    
    JsonArray dataArray = docCloud.createNestedArray(gatewayKey);
    JsonObject innerObj = dataArray.createNestedObject();
    
    innerObj["label_lokasi"]     = LOKASI_ALAT;
    innerObj["label_nama Modul"] = module_name;   
    innerObj["label_macAddr"]    = MAC_ADDRESS;  
    innerObj["data_kontrol"]     = data_ctrl;
    innerObj["data_status"]      = status_msg;
    innerObj["data_berita"]      = news;

    String payloadCloud;
    serializeJson(docCloud, payloadCloud);
    clientCloud.publish(TOPIC_CLOUD_GATEWAY, payloadCloud.c_str());
  }
}

// =========================================================================
// FUNGSI SETUP WI-FI MULTI
// =========================================================================
void setup_wifi() {
  delay(10);
  Serial.println("\n[WIFI] Memulai pemindaian sinyal Wi-Fi...");
  wifiMulti.addAP("RASV-SERVER", "Rasv-Server!123456"); 
  wifiMulti.addAP("Ifhan Sugiarto", "ifhans77");  
  wifiMulti.addAP("ayam", "12345678");            
  wifiMulti.addAP("Workshop 1", "eForacimenyan");
  wifiMulti.addAP("polban", "12polban34");       

  Serial.print("[WIFI] Menyambungkan");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n[WIFI] Sukses Terhubung Jaringan!");
  Serial.print("[WIFI] SSID Terhubung: ");
  Serial.println(WiFi.SSID());
}

// =========================================================================
// FUNGSI CALLBACKS RECEIVER
// =========================================================================
void processIncomingMessage(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == TOPIC_SUB_MODE) {
    if (msg == "AUTO") {
      modeOtomatis = true;
      Serial.println("\n[🎮 MODE CHANGED] Berpindah ke Mode: AUTONOMOUS AUTOMATIC.");
    } else if (msg == "MANUAL") {
      modeOtomatis = false;
      Serial.println("\n[🎮 MODE CHANGED] Berpindah ke Mode: OPERATOR FULL MANUAL CONTROL.");
    }
  }
  else if (String(topic) == TOPIC_SUB_CMD_REMOTE) {
    Serial.printf("\n[🎮 COMMAND RECV] Perintah Remote Masuk: %s\n", msg.c_str());
    if (msg == "RESET") {
      Serial.println("[🎮 SYSTEM RESET] Merestart ESP32 Master...");
      delay(1000);
      ESP.restart(); 
    } 
    else if (msg == "STOP") {
      if (currentState == STATE_SAMPLING) {
        Serial.println("[🎮 FORCE STOP] Memaksa Masuk Fase Flushing...");
        currentState = STATE_CALC_AND_FLUSH;
        subStateFlushing = 0;
      }
    }
  }
  else if (String(topic) == TOPIC_SUB_INTERVAL) {
    unsigned long interval_baru = msg.toInt();
    if (interval_baru >= 1000) { 
      intervalSampling = interval_baru;
      Serial.printf("[⚙️ INTERVAL CHANGED] Interval Sampling CO2: %lu ms\n", intervalSampling);
    }
  }
  else if (String(topic) == TOPIC_SUB_BARO) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) current_pressure_hpa = doc["data_kontrol"].as<float>();
    else current_pressure_hpa = msg.toFloat(); 
  }
  else if (String(topic) == TOPIC_SUB_TEMP) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) current_temp_c = doc["data_kontrol"].as<float>();
    else current_temp_c = msg.toFloat(); 
  }
  else if (String(topic) == TOPIC_SUB_STS_VALVE) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) {
      String ctrl = doc["data_kontrol"].as<String>();
      String status_msg = doc["data_status"].as<String>();
      if (ctrl == "0" && status_msg == "OK") { status_valve_vakum = 1; responValveDiterima = true; }
      else if (ctrl == "1" && status_msg == "OK") { status_valve_vakum = 0; responValveDiterima = true; }
    } else {
      status_valve_vakum = msg.toInt(); responValveDiterima = true;
    }
  }
  else if (String(topic) == TOPIC_SUB_STS_FAN) {
    status_kipas_pwm = msg.toInt();
  }
}

void callbackVPS(char* topic, byte* payload, unsigned int length) { processIncomingMessage(topic, payload, length); }
void callbackLocal(char* topic, byte* payload, unsigned int length) { processIncomingMessage(topic, payload, length); }

// =========================================================================
// FUNGSI REKONEKSI NON-BLOCKING
// =========================================================================
void maintainLocalConnection() {
  if (clientLocal.connected()) return;

  if (millis() - lastLocalTryTimer >= 5000) {
    lastLocalTryTimer = millis();
    Serial.print("[LOCAL] Memeriksa Server Lokal (192.168.15.85)...");
    
    if (clientLocal.connect(LOCAL_CLIENT_ID)) {
      Serial.println(" TERHUBUNG!");
      clientLocal.subscribe(TOPIC_SUB_BARO);
      clientLocal.subscribe(TOPIC_SUB_TEMP); 
      clientLocal.subscribe(TOPIC_SUB_STS_VALVE);
      clientLocal.subscribe(TOPIC_SUB_STS_FAN);
      clientLocal.subscribe(TOPIC_SUB_CMD_REMOTE);
      clientLocal.subscribe(TOPIC_SUB_INTERVAL);
      clientLocal.subscribe(TOPIC_SUB_MODE);
    } else {
      Serial.println(" OFF / Tidak Terjangkau.");
    }
  }
}

void checkAndConnectCloudServices() {
  if (clientVPS.connected() && clientCloud.connected()) return;

  // HANYA CEK TIAP 30 DETIK SEKALI
  if (millis() - lastCloudCheckTimer >= 30000 || lastCloudCheckTimer == 0) {
    lastCloudCheckTimer = millis();

    Serial.println("\n[NETWORK] Memeriksa ketersediaan internet via Smart DNS...");
    
    IPAddress vpsIP;
    if (WiFi.hostByName(VPS_HOST, vpsIP) == 1) {
      Serial.printf("[NETWORK] Internet Aktif! IP VPS: %s\n", vpsIP.toString().c_str());

      if (!clientVPS.connected()) {
        Serial.print("[VPS] Menghubungkan Master Node ke VPS...");
        if (clientVPS.connect(VPS_CLIENT_ID, VPS_USER, VPS_PASS)) {
          Serial.println(" TERHUBUNG!");
          clientVPS.subscribe(TOPIC_SUB_BARO);
          clientVPS.subscribe(TOPIC_SUB_TEMP); 
          clientVPS.subscribe(TOPIC_SUB_STS_VALVE);
          clientVPS.subscribe(TOPIC_SUB_STS_FAN);
          clientVPS.subscribe(TOPIC_SUB_CMD_REMOTE);
          clientVPS.subscribe(TOPIC_SUB_INTERVAL);
          clientVPS.subscribe(TOPIC_SUB_MODE);
        } else Serial.println(" GAGAL!");
      }

      if (!clientCloud.connected()) {
        Serial.print("[CLOUD] Menghubungkan Master Node ke Cloud Gateway...");
        if (clientCloud.connect(CLOUD_CLIENT_ID, CLOUD_USER, CLOUD_PASS)) {
          Serial.println(" TERHUBUNG!");
        } else Serial.println(" GAGAL!");
      }
    } else {
      Serial.println("[NETWORK] Router Offline (Tanpa Internet). Cloud Services Skipped.\n");
    }
  }
}

// =========================================================================
// RUN RUTIN INITIAL SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  setup_wifi(); 
  
  espClientVPS.setInsecure(); 
  espClientCloud.setInsecure();

  clientVPS.setServer(VPS_HOST, VPS_PORT);
  clientVPS.setCallback(callbackVPS);

  clientCloud.setServer(CLOUD_HOST, CLOUD_PORT);

  clientLocal.setServer(LOCAL_HOST, LOCAL_PORT);
  clientLocal.setCallback(callbackLocal);

  mhzSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  myMHZ19.begin(mhzSerial);
  myMHZ19.autoCalibration(false); 
  myMHZ19.setRange(5000); 

  Serial.println("\n[⚙️ SENSOR WARMUP] Menjalankan isolasi pra-pemanasan MH-Z19C selama 10 detik...");
  unsigned long timerWarmup = millis();
  while (millis() - timerWarmup < 10000) {
    int read_warm = myMHZ19.getCO2();
    if (read_warm > 0) current_co2 = (float)read_warm;
    Serial.printf("[WARMUP TIME] Sisa Waktu: %lu detik | PPM Terbaca: %.0f\n", 10 - ((millis() - timerWarmup)/1000), current_co2);
    delay(2000); 
  }
  Serial.println("[⚙️ SENSOR WARMUP] Sensor stabil! Masuk ke sistem kendali utama.\n");
}

// =========================================================================
// INTI PROGRAM KENDALI JALUR PROSES UTAMA (LOOP ENGINE)
// =========================================================================
void loop() {
  if (wifiMulti.run() != WL_CONNECTED) setup_wifi();

  maintainLocalConnection();

  if (clientLocal.connected()) clientLocal.loop();
  if (clientVPS.connected())   clientVPS.loop();
  if (clientCloud.connected()) clientCloud.loop();

  int ppm_baca = myMHZ19.getCO2();
  if (ppm_baca > 0) current_co2 = (float)ppm_baca;
  
  if (currentState == STATE_SAMPLING && (millis() - timerPublishLive >= 2000)) {
    timerPublishLive = millis();
    String strCO2  = String(current_co2, 0);
    String strDCDT = String(dCdt_sekarang, 4);

    publishJSONToVPSAndLocal(TOPIC_PUB_CO2, "Modul_CO2", strCO2, "Normal", modeOtomatis ? "Auto Sampling Active" : "Manual Monitoring Active");
    publishJSONToVPSAndLocal(TOPIC_PUB_DCDT, "Modul_dCdt", strDCDT, "Normal", "Live Gradient");

    publishGatewayCloud("Modul_CO2", strCO2, "Normal", modeOtomatis ? "Auto Sampling Active" : "Manual Monitoring Active");
    publishGatewayCloud("Modul_dCdt", strDCDT, "Normal", "Live Gradient");
  }

  switch (currentState) {
    
    case STATE_INIT_MEASURE:
      // PENGECEKAN INTERNET DIBATASI 30 DETIK SEKALI DI SINI
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

        String valvePayload = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Valve_Vaccum\",\"label_macAddr\":\"68:fe:71:87:47:c4\",\"data_kontrol\":\"1\",\"data_status\":\"OK\",\"data_berita\":\"Perintah Tutup Katup\"}";
        String fanPayload   = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Fan\",\"label_macAddr\":\"68:fe:71:13:1d:14\",\"data_kontrol\":\"100\",\"data_status\":\"OK\",\"data_berita\":\"Set Kipas Homogen\"}";
        
        if (clientLocal.connected()) {
          clientLocal.publish(TOPIC_PUB_CMD_VALVE, valvePayload.c_str());
          clientLocal.publish(TOPIC_PUB_CMD_FAN, fanPayload.c_str());
        }
        if (clientVPS.connected()) {
          clientVPS.publish(TOPIC_PUB_CMD_VALVE, valvePayload.c_str());
          clientVPS.publish(TOPIC_PUB_CMD_FAN, fanPayload.c_str());
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

        publishJSONToVPSAndLocal(TOPIC_PUB_FLUX, "Modul_Flux_Result", strFlux, "Calculated", "Nilai Fluks Maksimal Berhasil Dikunci");
        publishGatewayCloud("Modul_Flux_Result", strFlux, "Calculated", "Nilai Fluks Maksimal Berhasil Dikunci");
        
        if (modeOtomatis) {
          String fanOffPayload    = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Fan\",\"label_macAddr\":\"68:fe:71:13:1d:14\",\"data_kontrol\":\"0\",\"data_status\":\"OK\",\"data_berita\":\"Kipas Dimatikan Pra-Flushing\"}";
          String valveOpenPayload = "{\"label_lokasi\":\"Workshop\",\"label_nama Modul\":\"Modul_Actuator_Valve_Vaccum\",\"label_macAddr\":\"68:fe:71:87:47:c4\",\"data_kontrol\":\"0\",\"data_status\":\"OK\",\"data_berita\":\"Perintah Buka Katup & Flush\"}";
          
          if (clientLocal.connected()) {
            clientLocal.publish(TOPIC_PUB_CMD_FAN, fanOffPayload.c_str());
            clientLocal.publish(TOPIC_PUB_CMD_VALVE, valveOpenPayload.c_str());
          }
          if (clientVPS.connected()) {
            clientVPS.publish(TOPIC_PUB_CMD_FAN, fanOffPayload.c_str());
            clientVPS.publish(TOPIC_PUB_CMD_VALVE, valveOpenPayload.c_str());
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