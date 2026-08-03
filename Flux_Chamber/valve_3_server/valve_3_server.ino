/**
 * =========================================================================
 * Dokumen Kode Program: Node Eksekutor Katup & Pompa Vakum (ESP32)
 * Institusi: Politeknik Negeri Bandung (POLBAN)
 * Penulis: Ilham Muhammad Yusuf
 * Deskripsi: Kontrol Relay Katup & Vakum dengan Smart DNS Check & Non-Blocking Retry
 * =========================================================================
 */

#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>       

// =========================================================================
// 1. PINOUT DRIVER RELAY PERANGKAT KERAS
// =========================================================================
const int pinBuka   = 26; // Output relai motor: Buka Katup
const int pinTutup  = 25; // Output relai motor: Tutup Katup
const int pinVacuum = 33; // Output relai daya: Pompa Isap Vakum

// Identitas Metadata Perangkat
const char* LOKASI_ALAT = "Workshop";
const char* NAMA_MODUL  = "Modul_Actuator_Valve_Vaccum";
const char* MAC_ADDRESS = "68:fe:71:87:47:c4"; 

// =========================================================================
// 2. KREDENSIAL MULTI-WIFI (PRIORITAS ROUTER SERVER LOKAL)
// =========================================================================
WiFiMulti wifiMulti;

// =========================================================================
// 3. KONFIGURASI 3 BROKER MQTT (VPS, LOCAL, & CLOUD GATEWAY)
// =========================================================================

// --- BROKER 1: SERVER VPS (SSL Port 8883 - Full Pub & Sub) ---
const char* VPS_HOST      = "hivemqtt.e-farmingcorpora.cloud";
const int   VPS_PORT      = 8883;
const char* VPS_USER      = "efarming";
const char* VPS_PASS      = "EfarmingHiveMQ2026!";
const char* VPS_CLIENT_ID = "sensorvalve_node_vps";

const char* TOPIC_VPS_CMD_VALVE = "edufarm/flux/actuator/valve"; 
const char* TOPIC_VPS_STS_VALVE = "edufarm/flux/valve/status";   

// --- BROKER 2: SERVER LOCAL (Non-SSL Port 1883 - Full Pub & Sub) ---
const char* LOCAL_HOST      = "192.168.15.85"; // UPDATED IP SERVER LOKAL
const int   LOCAL_PORT      = 1883;
const char* LOCAL_CLIENT_ID = "sensorvalve_node_local";

const char* TOPIC_LOCAL_CMD_VALVE = "edufarm/flux/actuator/valve"; 
const char* TOPIC_LOCAL_STS_VALVE = "edufarm/flux/valve/status";   

// --- BROKER 3: SERVER CLOUD GATEWAY (SSL Port 8883 - Publish Status Only) ---
const char* CLOUD_HOST      = "c06a31d1daa24624a3b0340899bf4fe3.s1.eu.hivemq.cloud"; 
const int   CLOUD_PORT      = 8883;
const char* CLOUD_USER      = "dortek2026_consumer_wsan";                  
const char* CLOUD_PASS      = "DORtek2026";                  
const char* CLOUD_CLIENT_ID = "sensorvalve_node_cloud"; 

const char* TOPIC_CLOUD_GATEWAY = "v1/gateway/data";

// =========================================================================
// 4. INSTANSIASI CLIENTS & TIMER NON-BLOCKING
// =========================================================================
WiFiClientSecure espClientVPS;
PubSubClient clientVPS(espClientVPS);

WiFiClient espClientLocal; // Client Biasa (Non-SSL)
PubSubClient clientLocal(espClientLocal);

WiFiClientSecure espClientCloud;
PubSubClient clientCloud(espClientCloud);

// Timer Proteksi Non-Blocking (Pencegah Freeze DNS)
unsigned long lastVPSTryTimer   = 0;
unsigned long lastCloudTryTimer = 0;
unsigned long lastLocalTryTimer = 0;

// =========================================================================
// FUNGSI: PUBLIKASI STATUS HANDSHAKE KE SELURUH BROKER AKTIF
// =========================================================================
void publishStatusAllServers(String data_kontrol, String data_status, String data_berita) {
  // 1. PUBLISH KE LOCAL
  if (clientLocal.connected()) {
    StaticJsonDocument<256> docLocal;
    docLocal["label_lokasi"]     = LOKASI_ALAT;
    docLocal["label_nama Modul"] = NAMA_MODUL;   
    docLocal["label_macAddr"]    = MAC_ADDRESS;  
    docLocal["data_kontrol"]     = data_kontrol; 
    docLocal["data_status"]      = data_status;  
    docLocal["data_berita"]      = data_berita;  

    String payloadLocal;
    serializeJson(docLocal, payloadLocal);
    clientLocal.publish(TOPIC_LOCAL_STS_VALVE, payloadLocal.c_str());
    Serial.printf("[LOCAL STS PUB] -> %s\n", payloadLocal.c_str());
  }

  // 2. PUBLISH KE VPS
  if (clientVPS.connected()) {
    StaticJsonDocument<256> docVPS;
    docVPS["label_lokasi"]     = LOKASI_ALAT;
    docVPS["label_nama Modul"] = NAMA_MODUL;   
    docVPS["label_macAddr"]    = MAC_ADDRESS;  
    docVPS["data_kontrol"]     = data_kontrol; 
    docVPS["data_status"]      = data_status;  
    docVPS["data_berita"]      = data_berita;  

    String payloadVPS;
    serializeJson(docVPS, payloadVPS);
    clientVPS.publish(TOPIC_VPS_STS_VALVE, payloadVPS.c_str());
    Serial.printf("[VPS STS PUB] -> %s\n", payloadVPS.c_str());
  }

  // 3. PUBLISH KE CLOUD GATEWAY
  if (clientCloud.connected()) {
    StaticJsonDocument<512> docCloud;
    String gatewayKey = "ACTUATOR_VALVE_" + String(LOKASI_ALAT) + "_" + String(MAC_ADDRESS);
    
    JsonArray dataArray = docCloud.createNestedArray(gatewayKey);
    JsonObject innerObj = dataArray.createNestedObject();
    
    innerObj["label_lokasi"]     = LOKASI_ALAT;
    innerObj["label_nama Modul"] = NAMA_MODUL;   
    innerObj["label_macAddr"]    = MAC_ADDRESS;  
    innerObj["data_kontrol"]     = data_kontrol; 
    innerObj["data_status"]      = data_status;  
    innerObj["data_berita"]      = data_berita;  

    String payloadCloud;
    serializeJson(docCloud, payloadCloud);
    clientCloud.publish(TOPIC_CLOUD_GATEWAY, payloadCloud.c_str());
    Serial.printf("[CLOUD GATEWAY PUB] -> %s\n", payloadCloud.c_str());
  }
}

// =========================================================================
// FUNGSI EKSEKUSI PEMPROSESAN PERINTAH KATUP & VAKUM
// =========================================================================
void processValveCommand(String msg) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, msg);

  String commandValue = "";
  if (error) {
    commandValue = msg; 
  } else {
    commandValue = doc["data_kontrol"].as<String>(); 
  }

  commandValue.trim();
  Serial.printf("[VALVE RECV] Eksekusi Komando: %s\n", commandValue.c_str());

  if (commandValue == "1") { 
    Serial.println("[MECHANICAL] Menjalankan Aktuator: PROSES MENUTUP KATUP...");
    digitalWrite(pinVacuum, LOW);    
    digitalWrite(pinBuka, LOW);      
    digitalWrite(pinTutup, HIGH);    
    delay(7500);                     
    digitalWrite(pinTutup, LOW);     
    
    publishStatusAllServers("1", "OK", "Kubah Tertutup Rapat. Fase Sampling Dimulai.");
    Serial.println("[VALVE REPORT] Status 0 (Rapat) Terkirim.");
  } 
  else if (commandValue == "0") {
    Serial.println("[MECHANICAL] Menjalankan Aktuator: PROSES MEMBUKA KATUP & VAKUM ON...");
    digitalWrite(pinTutup, LOW);     
    digitalWrite(pinBuka, HIGH);     
    delay(7000);                     
    digitalWrite(pinBuka, LOW);      
    
    digitalWrite(pinVacuum, HIGH);   
    
    publishStatusAllServers("0", "OK", "Katup Terbuka Lebar. Pompa Vakum Aktif Membersihkan.");
    Serial.println("[VALVE REPORT] Status 1 (Terbuka) Terkirim.");
  }
}

// =========================================================================
// FUNGSI CALLBACKS RECEIVER SINKRON (VPS & LOCAL)
// =========================================================================
void callbackVPS(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_VPS_CMD_VALVE) {
    processValveCommand(msg);
  }
}

void callbackLocal(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_LOCAL_CMD_VALVE) {
    processValveCommand(msg);
  }
}

// =========================================================================
// FUNGSI SETUP WI-FI MULTI
// =========================================================================
void setup_wifi() {
  delay(10);
  Serial.println("\n[WIFI] Memulai pemindaian Wi-Fi...");

  Serial.print("[WIFI] Menyambungkan ke titik jaringan terbaik");
  while (wifiMulti.run() != WL_CONNECTED) { 
    delay(500); Serial.print("."); 
  }
  Serial.println("\n[WIFI] Terhubung!");
  Serial.print("[WIFI] SSID Terhubung: ");
  Serial.println(WiFi.SSID());
}

// =========================================================================
// FUNGSI REKONEKSI NON-BLOCKING DENGAN SMART DNS CHECK
// =========================================================================
void maintainLocalConnection() {
  if (clientLocal.connected()) return;

  if (millis() - lastLocalTryTimer >= 5000) {
    lastLocalTryTimer = millis();
    Serial.print("[LOCAL] Memeriksa Server Lokal (192.168.15.85)...");
    
    if (clientLocal.connect(LOCAL_CLIENT_ID)) {
      Serial.println(" TERHUBUNG!");
      clientLocal.subscribe(TOPIC_LOCAL_CMD_VALVE);
    } else {
      Serial.println(" OFF / Tidak Terjangkau.");
    }
  }
}

void maintainVPSConnection() {
  if (clientVPS.connected()) return;

  // Coba hubungi VPS tiap 60 detik sekali
  if (millis() - lastVPSTryTimer >= 60000) {
    lastVPSTryTimer = millis();
    
    IPAddress dummyIP;
    if (WiFi.hostByName(VPS_HOST, dummyIP) == 1) {
      Serial.print("[VPS] Internet Aktif! Menghubungkan Valve Node ke VPS...");
      if (clientVPS.connect(VPS_CLIENT_ID, VPS_USER, VPS_PASS)) {
        Serial.println(" TERHUBUNG!");
        clientVPS.subscribe(TOPIC_VPS_CMD_VALVE); 
      } else {
        Serial.println(" GAGAL!");
      }
    } else {
      Serial.println("[VPS] No Internet. (VPS Skipped Instant)");
    }
  }
}

void maintainCloudConnection() {
  if (clientCloud.connected()) return;

  // Coba hubungi Cloud Gateway tiap 60 detik sekali
  if (millis() - lastCloudTryTimer >= 60000) {
    lastCloudTryTimer = millis();
    
    IPAddress dummyIP;
    if (WiFi.hostByName(CLOUD_HOST, dummyIP) == 1) {
      Serial.print("[CLOUD] Internet Aktif! Menghubungkan Valve Node ke Cloud Gateway...");
      if (clientCloud.connect(CLOUD_CLIENT_ID, CLOUD_USER, CLOUD_PASS)) {
        Serial.println(" TERHUBUNG!");
      } else {
        Serial.println(" GAGAL!");
      }
    } else {
      Serial.println("[CLOUD] No Internet. (Cloud Skipped Instant)");
    }
  }
}

// =========================================================================
// RUN INITIAL SETUP PERIFERAL HARDWARE & KOMUNIKASI
// =========================================================================
void setup() {
  Serial.begin(115200);

  pinMode(pinBuka, OUTPUT);
  pinMode(pinTutup, OUTPUT);
  pinMode(pinVacuum, OUTPUT);
  
  digitalWrite(pinBuka, LOW);
  digitalWrite(pinTutup, LOW);
  digitalWrite(pinVacuum, LOW);

  setup_wifi();
  
  espClientVPS.setInsecure(); 
  espClientCloud.setInsecure();

  clientVPS.setServer(VPS_HOST, VPS_PORT);
  clientVPS.setCallback(callbackVPS);

  clientCloud.setServer(CLOUD_HOST, CLOUD_PORT);

  clientLocal.setServer(LOCAL_HOST, LOCAL_PORT);
  clientLocal.setCallback(callbackLocal);
}

// =========================================================================
// LOOP UTAMA MONITORING KONTROL KATUP & VAKUM
// =========================================================================
void loop() {
  if (wifiMulti.run() != WL_CONNECTED) setup_wifi();

  maintainLocalConnection(); // LAN Direct IP
  maintainVPSConnection();   // Smart DNS check tiap 60 detik
  maintainCloudConnection(); // Smart DNS check tiap 60 detik

  if (clientLocal.connected()) clientLocal.loop();
  if (clientVPS.connected())   clientVPS.loop();
  if (clientCloud.connected()) clientCloud.loop();
}