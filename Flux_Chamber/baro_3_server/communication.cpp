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

unsigned long lastVPSTryTimer   = 0;
unsigned long lastLocalTryTimer = 0;

// Variabel flag pelindung memori antar-Core (Thread Safety)
volatile bool isVPSConnecting = false;

void processIntervalCommand(String msg) {
  unsigned long interval_baru = msg.toInt();
  if (interval_baru >= 1000) { 
    intervalPublishBaro = interval_baru;
    Serial.printf("\n[⚙️ INTERVAL CHANGED] Ritme Pengiriman BME280 Berubah: %lu ms (%.1f detik)\n", 
                  intervalPublishBaro, intervalPublishBaro / 1000.0);
  }
}

void callbackVPS(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_VPS_SUB_INTERVAL) processIntervalCommand(msg);
}

void callbackLocal(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_LOCAL_SUB_INTERVAL) processIntervalCommand(msg);
}

void setup_wifi() {
  delay(10);
  Serial.println("\n[WIFI] Memulai pemindaian Wi-Fi...");
  
  wifiMulti.addAP("RASV-SERVER", "Rasv-Server!123456"); 
  wifiMulti.addAP("Ifhan Sugiarto", "ifhans77");  
  wifiMulti.addAP("Workshop 1", "eForacimenyan");

  Serial.print("[WIFI] Menyambungkan ke titik jaringan terbaik");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n[WIFI] Terhubung!");
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
      clientLocal.subscribe(TOPIC_LOCAL_SUB_INTERVAL);
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
    // Hanya mencoba koneksi jika Wi-Fi sudah tersambung dan VPS sedang terputus
    if (WiFi.status() == WL_CONNECTED && !clientVPS.connected()) {
      
      if (millis() - lastVPSTryTimer >= 10000) {
        lastVPSTryTimer = millis();
        isVPSConnecting = true; // Kunci akses clientVPS dari Core 1
        
        IPAddress dummyIP;
        if (WiFi.hostByName(VPS_HOST, dummyIP) == 1) {
          Serial.print("[VPS-TASK] Internet Aktif! Menghubungkan ke VPS...");
          if (clientVPS.connect(VPS_CLIENT_ID, VPS_USER, VPS_PASS)) {
            Serial.println(" TERHUBUNG KEMBALI!");
            clientVPS.subscribe(TOPIC_VPS_SUB_INTERVAL);
          } else {
            Serial.println(" GAGAL!");
          }
        } else {
          Serial.println("[VPS-TASK] No Internet. (VPS Skipped Instant)");
        }
        
        isVPSConnecting = false; // Buka kembali akses clientVPS untuk Core 1
      }
    }
    // Memberikan jeda waktu (yield) agar Core 0 tidak kepanasan
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
  }
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

  // Memicu Tugas Paralel di Core 0 (PRO_CPU)
  xTaskCreatePinnedToCore(
    vpsConnectionTask,   // Nama fungsi Task
    "Task_VPS_Reconnect",// Label Task
    8192,                // Kapasitas RAM (SSL butuh memori besar)
    NULL,                // Parameter input
    1,                   // Prioritas eksekusi
    NULL,                // Task Handler
    0                    // Pin ke Core 0
  );
}

void maintainCommunication() {
  if (wifiMulti.run() != WL_CONNECTED) setup_wifi();
  
  maintainLocalConnection(); 
  // maintainVPSConnection(); // FUNGSI INI TELAH DIPINDAHKAN KE CORE 0

  if (clientLocal.connected()) {
    clientLocal.loop();
  }
  
  // Pastikan Core 0 tidak sedang menggunakan objek clientVPS sebelum melakukan loop
  if (clientVPS.connected() && !isVPSConnecting) {
    clientVPS.loop();
  }
}

void publishBaroData(float suhu, float tekanan) {
  String stringTemp = String(suhu, 2);
  String stringBaro = String(tekanan, 2);
  String rootKey    = "sensorNode_farmda27b11b_" + String(MAC_ADDRESS);

  // -------------------------------------------------------------------
  // 1. PUBLISH KE SERVER LOCAL (Tanpa hambatan, selalu tereksekusi)
  // -------------------------------------------------------------------
  if (clientLocal.connected()) {
    StaticJsonDocument<256> docLocalTemp;
    docLocalTemp["label_lokasi"]     = LOKASI_ALAT;
    docLocalTemp["label_nama Modul"] = "Modul_Baro_Temp";
    docLocalTemp["label_macAddr"]    = MAC_ADDRESS;
    docLocalTemp["data_kontrol"]     = stringTemp;
    docLocalTemp["data_status"]      = "Normal";
    docLocalTemp["data_berita"]      = "Data Suhu Kubah";
    
    String payloadLocalTemp;
    serializeJson(docLocalTemp, payloadLocalTemp);
    clientLocal.publish(TOPIC_LOCAL_PUB_TEMP, payloadLocalTemp.c_str());

    StaticJsonDocument<256> docLocalBaro;
    docLocalBaro["label_lokasi"]     = LOKASI_ALAT;
    docLocalBaro["label_nama Modul"] = "Modul_Baro_Pres";
    docLocalBaro["label_macAddr"]    = MAC_ADDRESS;
    docLocalBaro["data_kontrol"]     = stringBaro;
    docLocalBaro["data_status"]      = "Normal";
    docLocalBaro["data_berita"]      = "Data Tekanan Barometrik";

    String payloadLocalBaro;
    serializeJson(docLocalBaro, payloadLocalBaro);
    clientLocal.publish(TOPIC_LOCAL_PUB_BARO, payloadLocalBaro.c_str());

    Serial.printf("[LOCAL PUB] Suhu: %s C | Tekanan: %s hPa\n", stringTemp.c_str(), stringBaro.c_str());
  }

  // -------------------------------------------------------------------
  // 2. PUBLISH KE SERVER VPS (Aman dari bentrok antar-Core)
  // -------------------------------------------------------------------
  if (clientVPS.connected() && !isVPSConnecting) {
    StaticJsonDocument<768> docVPSTemp;
    JsonArray arrTemp = docVPSTemp.createNestedArray(rootKey);
    JsonObject objTemp = arrTemp.createNestedObject();
    
    objTemp["label_lokasi"]     = "farmda27b11b";
    objTemp["label_nama_Modul"] = "sensor_bme280";
    objTemp["label_variabel"]   = "suhu";
    objTemp["label_satuan"]     = "C";
    objTemp["label_macAddr"]    = MAC_ADDRESS;
    objTemp["data_kontrol"]     = suhu; 
    objTemp["data_status"]      = "normal";
    objTemp["data_berita"]      = "Data Suhu Kubah";
    
    String payloadVPSTemp;
    serializeJson(docVPSTemp, payloadVPSTemp);

    clientVPS.publish("farmda27b11b", payloadVPSTemp.c_str());
    delay(10); clientVPS.loop();
    clientVPS.publish(TOPIC_VPS_PUB_TEMP, payloadVPSTemp.c_str());
    delay(10); clientVPS.loop();

    StaticJsonDocument<768> docVPSBaro;
    JsonArray arrBaro = docVPSBaro.createNestedArray(rootKey);
    JsonObject objBaro = arrBaro.createNestedObject();
    
    objBaro["label_lokasi"]     = "farmda27b11b";
    objBaro["label_nama_Modul"] = "sensor_bme280";
    objBaro["label_variabel"]   = "tekanan";
    objBaro["label_satuan"]     = "hPa";
    objBaro["label_macAddr"]    = MAC_ADDRESS;
    objBaro["data_kontrol"]     = tekanan; 
    objBaro["data_status"]      = "normal";
    objBaro["data_berita"]      = "Data Tekanan Barometrik";
    
    String payloadVPSBaro;
    serializeJson(docVPSBaro, payloadVPSBaro);

    clientVPS.publish("farmda27b11b", payloadVPSBaro.c_str());
    delay(10); clientVPS.loop();
    clientVPS.publish(TOPIC_VPS_PUB_BARO, payloadVPSBaro.c_str());
    
    Serial.println("[VPS PUB] Data Bersarang Terkirim ke VPS.");
  }
}