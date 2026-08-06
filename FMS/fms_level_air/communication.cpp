#include "communication.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h> 
#include <WiFiClient.h> 
#include <ArduinoJson.h>
#include <esp_task_wdt.h> // Library Watchdog pengaman

WiFiMulti wifiMulti;

// --- Klien VPS (SSL/TLS) ---
WiFiClientSecure espClientVPS;
PubSubClient clientVPS(espClientVPS);

// --- Klien Lokal (Non-SSL) ---
WiFiClient espClientLocal;
PubSubClient clientLocal(espClientLocal);

unsigned long lastVPSTryTimer   = 0;
unsigned long lastLocalTryTimer = 0;

// Variabel flag pelindung memori antar-Core (Thread Safety)
volatile bool isVPSConnecting = false;

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
    delay(500); 
    Serial.print(".");
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
    } else {
      Serial.println(" OFF / Tidak Terjangkau.");
    }
  }
}

// =========================================================================
// FREERTOS TASK: BERJALAN PARALEL DI CORE 0 (BACKGROUND)
// =========================================================================
void vpsConnectionTask(void * parameter) {
  for(;;) {
    // Hanya mencoba koneksi jika Wi-Fi tersambung dan VPS terputus
    if (WiFi.status() == WL_CONNECTED && !clientVPS.connected()) {
      
      if (millis() - lastVPSTryTimer >= 10000) {
        lastVPSTryTimer = millis();
        isVPSConnecting = true; // Kunci akses clientVPS dari Core 1
        
        // Pengecekan Ketersediaan Internet (DNS Ping)
        IPAddress dummyIP;
        if (WiFi.hostByName(VPS_HOST, dummyIP) == 1) {
          Serial.print("[VPS-TASK] Internet Aktif! Menghubungkan ke VPS...");
          
          // Mematikan sementara WDT saat enkripsi SSL (Mencegah Task WDT Triggered)
          disableCore0WDT();
          bool isConnected = clientVPS.connect(VPS_CLIENT_ID, VPS_USER, VPS_PASS);
          enableCore0WDT();
          
          if (isConnected) {
            Serial.println(" TERHUBUNG KEMBALI!");
          } else {
            Serial.print(" GAGAL! rc=");
            Serial.println(clientVPS.state());
            espClientVPS.stop(); // Pembersihan agresif memori soket
          }
        } else {
          Serial.println("[VPS-TASK] No Internet. (VPS Skipped Instant)");
        }
        
        isVPSConnecting = false; // Buka kembali akses clientVPS untuk Core 1
      }
    }
    // Memberikan jeda agar Core 0 tidak kepanasan & me-reset Watchdog
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
  }
}

void setupCommunication() {
  setup_wifi(); 
  
  espClientVPS.setInsecure(); 
  espClientVPS.setTimeout(5);          // Batas penantian data (Mencegah Deadlock)
  espClientVPS.setHandshakeTimeout(5); // Batas penantian koneksi SSL

  clientVPS.setServer(VPS_HOST, VPS_PORT);
  clientVPS.setBufferSize(1024); 

  clientLocal.setServer(LOCAL_HOST, LOCAL_PORT);
  clientLocal.setBufferSize(1024); 

  // Memicu Tugas Paralel di Core 0 (PRO_CPU)
  xTaskCreatePinnedToCore(
    vpsConnectionTask,   
    "Task_VPS_Reconnect",
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

  if (clientLocal.connected()) {
    clientLocal.loop();
  }
  
  // Pastikan Core 0 tidak sedang menyambungkan VPS sebelum mengeksekusi loop
  if (clientVPS.connected() && !isVPSConnecting) {
    clientVPS.loop();
  }
}

void publishJarakData(String data_kontrol) {
  String rootKey = "sensorNode_farmda27b11b_" + String(MAC_ADDRESS);

  // -------------------------------------------------------------------
  // 1. PUBLISH KE SERVER LOCAL (Tanpa Hambatan)
  // -------------------------------------------------------------------
  if (clientLocal.connected()) {
    StaticJsonDocument<512> docLocal;
    docLocal["label_lokasi"]     = LOKASI_ALAT;
    docLocal["label_nama_Modul"] = "Modul_Ultrasonic";
    docLocal["label_variabel"]   = "jarak";
    docLocal["label_satuan"]     = "cm";
    docLocal["label_macAddr"]    = MAC_ADDRESS;
    docLocal["data_kontrol"]     = data_kontrol.toFloat();
    docLocal["data_status"]      = "normal";
    docLocal["data_berita"]      = "Data Jarak Permukaan Aktual";

    String payloadLocal;
    serializeJson(docLocal, payloadLocal);
    clientLocal.publish(TOPIC_PUB_JARAK, payloadLocal.c_str());
  }

  // -------------------------------------------------------------------
  // 2. PUBLISH KE SERVER VPS (Aman dari bentrok Core 0)
  // -------------------------------------------------------------------
  if (clientVPS.connected() && !isVPSConnecting) {
    StaticJsonDocument<768> docVPS;
    JsonArray arr = docVPS.createNestedArray(rootKey);
    JsonObject obj = arr.createNestedObject();
    
    obj["label_lokasi"]     = "farmda27b11b";
    obj["label_nama_Modul"] = "Modul_Ultrasonic";
    obj["label_variabel"]   = "jarak";
    obj["label_satuan"]     = "cm";
    obj["label_macAddr"]    = MAC_ADDRESS;
    obj["data_kontrol"]     = data_kontrol.toFloat(); 
    obj["data_status"]      = "normal";
    obj["data_berita"]      = "Data Jarak Permukaan Aktual";
    
    String payloadVPS;
    serializeJson(docVPS, payloadVPS);

    clientVPS.publish(TOPIC_PUB_JARAK, payloadVPS.c_str());
  }
}