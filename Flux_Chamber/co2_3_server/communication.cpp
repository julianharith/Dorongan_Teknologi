#include "communication.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>

WiFiMulti wifiMulti;

WiFiClientSecure espClientVPS;
PubSubClient clientVPS(espClientVPS);

WiFiClient espClientLocal;
PubSubClient clientLocal(espClientLocal);

unsigned long lastLocalTryTimer = 0;
unsigned long lastCloudCheckTimer = 0;

// Flag pelindung memori antar-Core (Thread Safety)
volatile bool isVPSConnecting = false;

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

void maintainLocalConnection() {
  if (clientLocal.connected()) return;

  if (millis() - lastLocalTryTimer >= 5000) {
    lastLocalTryTimer = millis();
    Serial.print("[LOCAL] Memeriksa Server Lokal (192.168.0.130)...");
    
    if (clientLocal.connect(LOCAL_CLIENT_ID, LOCAL_USER, LOCAL_PASS)) {
      Serial.println(" TERHUBUNG!");
      clientLocal.subscribe(TOPIC_SUB_BARO);
      clientLocal.subscribe(TOPIC_SUB_TEMP); 
      clientLocal.subscribe(TOPIC_SUB_STS_VALVE);
      clientLocal.subscribe(TOPIC_SUB_STS_FAN);
      clientLocal.subscribe(TOPIC_SUB_CMD_REMOTE);
      clientLocal.subscribe(TOPIC_SUB_INTERVAL);
      clientLocal.subscribe(TOPIC_SUB_MODE);
    } else {
      Serial.println(" OFF / Tidak Terjangkau atau Gagal Autentikasi.");
    }
  }
}

// =========================================================================
// FREERTOS TASK: BERJALAN PARALEL DI CORE 0 (BACKGROUND)
// =========================================================================
void vpsConnectionTask(void * parameter) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED && !clientVPS.connected()) {
      if (millis() - lastCloudCheckTimer >= 10000) {
        lastCloudCheckTimer = millis();
        isVPSConnecting = true; // Kunci akses
        
        IPAddress vpsIP;
        if (WiFi.hostByName(VPS_HOST, vpsIP) == 1) {
          Serial.print("[VPS-TASK] Internet Aktif! Menghubungkan Master ke VPS...");
          if (clientVPS.connect(VPS_CLIENT_ID, VPS_USER, VPS_PASS)) {
            Serial.println(" TERHUBUNG KEMBALI!");
            clientVPS.subscribe(TOPIC_SUB_BARO);
            clientVPS.subscribe(TOPIC_SUB_TEMP); 
            clientVPS.subscribe(TOPIC_SUB_STS_VALVE);
            clientVPS.subscribe(TOPIC_SUB_STS_FAN);
            clientVPS.subscribe(TOPIC_SUB_CMD_REMOTE);
            clientVPS.subscribe(TOPIC_SUB_INTERVAL);
            clientVPS.subscribe(TOPIC_SUB_MODE);
          } else {
            Serial.printf(" GAGAL! (Code: %d)\n", clientVPS.state());
          }
        } else {
          Serial.println("[VPS-TASK] No Internet. (VPS Skipped)");
        }
        isVPSConnecting = false; // Buka kunci akses
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
  }
}

void checkAndConnectCloudServices() {
  // Tidak lagi menggunakan pemanggilan langsung, karena diambil alih Core 0
  // Fungsi ini dibiarkan kosong agar kompatibel dengan pemanggilan di file utama
}

void setupCommunication() {
  setup_wifi(); 
  espClientVPS.setInsecure(); 

  clientVPS.setServer(VPS_HOST, VPS_PORT);
  clientVPS.setCallback(callbackVPS);
  clientVPS.setBufferSize(1024); 

  clientLocal.setServer(LOCAL_HOST, LOCAL_PORT);
  clientLocal.setCallback(callbackLocal);
  clientLocal.setBufferSize(1024); 

  // Aktifkan FreeRTOS Task di Core 0
  xTaskCreatePinnedToCore(
    vpsConnectionTask,
    "Task_VPS_Master",
    8192,
    NULL,
    1,
    NULL,
    0
  );
}

void maintainCommunication() {
  if (wifiMulti.run() != WL_CONNECTED) setup_wifi();
  
  maintainLocalConnection();
  
  if (clientLocal.connected()) clientLocal.loop();
  
  // Hanya jalankan loop VPS jika tidak sedang proses reconnect
  if (clientVPS.connected() && !isVPSConnecting) clientVPS.loop();
}

void publishJSONToVPSAndLocal(const char* topic, const char* nama_modul, String variabel, String satuan, String data_kontrol, String data_status, String data_berita) {
  // --- 1. LOKAL ---
  StaticJsonDocument<512> docLocal;
  docLocal["label_lokasi"]     = LOKASI_ALAT;
  docLocal["label_nama Modul"] = nama_modul;   
  docLocal["label_macAddr"]    = MAC_ADDRESS;  
  docLocal["data_kontrol"]     = data_kontrol; 
  docLocal["data_status"]      = data_status;
  docLocal["data_berita"]      = data_berita;

  String payloadLocal;
  serializeJson(docLocal, payloadLocal);

  // --- 2. VPS ---
  StaticJsonDocument<768> docVPS;
  String rootKey = "sensorNode_farmda27b11b_" + String(MAC_ADDRESS);
  
  JsonArray dataArray = docVPS.createNestedArray(rootKey);
  JsonObject innerObj = dataArray.createNestedObject();

  innerObj["label_lokasi"]     = "farmda27b11b"; 
  innerObj["label_nama_Modul"] = "sensor_co2";   
  innerObj["label_variabel"]   = variabel;
  innerObj["label_satuan"]     = satuan;
  innerObj["label_macAddr"]    = MAC_ADDRESS;  
  innerObj["data_kontrol"]     = data_kontrol.toFloat(); 
  innerObj["data_status"]      = data_status;
  innerObj["data_berita"]      = data_berita;

  String payloadVPS;
  serializeJson(docVPS, payloadVPS);

  if (clientLocal.connected()) {
    clientLocal.publish(topic, payloadLocal.c_str());
  }
  
  // Sinkronisasi keamanan thread
  if (clientVPS.connected() && !isVPSConnecting) {
    bool sendCustom = clientVPS.publish("farmda27b11b", payloadVPS.c_str());
    delay(10);
    clientVPS.loop();
    bool sendStandard = clientVPS.publish(topic, payloadVPS.c_str());
    Serial.printf("[MQTT VPS] Publish Status -> Custom: %d | Standar: %d\n", sendCustom, sendStandard);
  }
}