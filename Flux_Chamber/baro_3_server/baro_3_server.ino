/**
 * =========================================================================
 * Dokumen Kode Program: Node Telemetri Barometrik & Suhu (BME280)
 * Institusi: Politeknik Negeri Bandung (POLBAN)
 * Jurusan: Teknik Elektro / D4 - Teknik Elektronika
 * Penulis: Ilham Muhammad Yusuf (NIM: 221354046)
 * Deskripsi: Akuisisi parameter tekanan barometrik & suhu via BME280 (I2C)
 * dengan arsitektur Offline-First (Smart DNS Check & Non-Blocking Retry).
 * =========================================================================
 */

#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>       
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// =========================================================================
// 1. PINOUT BUS I2C CUSTOM & SENSOR BME280
// =========================================================================
#define PIN_SDA 21
#define PIN_SCL 22

Adafruit_BME280 bme; 

// Identitas Metadata Perangkat
const char* LOKASI_ALAT = "Workshop";
const char* MAC_ADDRESS = "68:fe:71:13:02:d8"; 

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
const char* VPS_CLIENT_ID = "sensorbaro_node_vps";

// Topik VPS
const char* TOPIC_VPS_PUB_TEMP       = "edufarm/flux/sensor/temperature"; 
const char* TOPIC_VPS_PUB_BARO       = "edufarm/flux/sensor/baro";        
const char* TOPIC_VPS_SUB_INTERVAL   = "edufarm/flux/sensor/baro/interval"; 

// --- BROKER 2: SERVER LOCAL (Non-SSL Port 1883 - Full Pub & Sub) ---
const char* LOCAL_HOST      = "192.168.15.85"; // UPDATED IP SERVER LOKAL
const int   LOCAL_PORT      = 1883;
const char* LOCAL_CLIENT_ID = "sensorbaro_node_local";

// Topik Local
const char* TOPIC_LOCAL_PUB_TEMP     = "edufarm/flux/sensor/temperature"; 
const char* TOPIC_LOCAL_PUB_BARO     = "edufarm/flux/sensor/baro";        
const char* TOPIC_LOCAL_SUB_INTERVAL = "edufarm/flux/sensor/baro/interval"; 

// --- BROKER 3: SERVER CLOUD GATEWAY (SSL Port 8883 - Publish Only) ---
const char* CLOUD_HOST      = "c06a31d1daa24624a3b0340899bf4fe3.s1.eu.hivemq.cloud"; 
const int   CLOUD_PORT      = 8883; 
const char* CLOUD_USER      = "dortek2026_consumer_wsan";                  
const char* CLOUD_PASS      = "DORtek2026";                  
const char* CLOUD_CLIENT_ID = "sensorbaro_node_cloud"; 

// Topik Cloud Gateway
const char* TOPIC_CLOUD_GATEWAY     = "v1/gateway/data";

// =========================================================================
// 4. INSTANSIASI CLIENTS & TIMER NON-BLOCKING
// =========================================================================
WiFiClientSecure espClientVPS;
PubSubClient clientVPS(espClientVPS);

WiFiClient espClientLocal; // Client Biasa (Non-SSL)
PubSubClient clientLocal(espClientLocal);

WiFiClientSecure espClientCloud;
PubSubClient clientCloud(espClientCloud);

unsigned long timerPublish = 0;
unsigned long intervalPublishBaro = 2000; // Default interval pengiriman: 2 Detik

// Timer Proteksi Non-Blocking
unsigned long lastVPSTryTimer   = 0;
unsigned long lastCloudTryTimer = 0;
unsigned long lastLocalTryTimer = 0;

// =========================================================================
// FUNGSI CALLBACK: PROSES PESAN MASUK DARI VPS ATAU LOCAL
// =========================================================================
void processIntervalCommand(String msg) {
  unsigned long interval_baru = msg.toInt();
  if (interval_baru >= 1000) { // Limit minimal interval 1 detik
    intervalPublishBaro = interval_baru;
    Serial.printf("\n[⚙️ INTERVAL CHANGED] Ritme Pengiriman BME280 Berubah: %lu ms (%.1f detik)\n", 
                  intervalPublishBaro, intervalPublishBaro / 1000.0);
  }
}

void callbackVPS(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_VPS_SUB_INTERVAL) {
    processIntervalCommand(msg);
  }
}

void callbackLocal(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_LOCAL_SUB_INTERVAL) {
    processIntervalCommand(msg);
  }
}

// =========================================================================
// FUNGSI SETUP WI-FI MULTI (PRIORITAS ROUTER SERVER LOKAL)
// =========================================================================
void setup_wifi() {
  delay(10);
  Serial.println("\n[WIFI] Memulai pemindaian Wi-Fi...");
  
  // PRIORITAS 1: Router Server Lokal
  wifiMulti.addAP("RASV-SERVER", "Rasv-Server!123456"); 

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

  // Coba hubungi server lokal tiap 5 detik sekali (Direct IP LAN)
  if (millis() - lastLocalTryTimer >= 5000) {
    lastLocalTryTimer = millis();
    Serial.print("[LOCAL] Memeriksa Server Lokal (192.168.15.85)...");
    if (clientLocal.connect(LOCAL_CLIENT_ID)) {
      Serial.println(" TERHUBUNG!");
      clientLocal.subscribe(TOPIC_LOCAL_SUB_INTERVAL);
    } else {
      Serial.println(" OFF / Tidak Terjangkau.");
    }
  }
}

void maintainVPSConnection() {
  if (clientVPS.connected()) return;

  // Coba hubungi VPS tiap 60 detik sekali agar tidak menahan loop saat offline
  if (millis() - lastVPSTryTimer >= 60000) {
    lastVPSTryTimer = millis();
    
    IPAddress dummyIP;
    if (WiFi.hostByName(VPS_HOST, dummyIP) == 1) {
      Serial.print("[VPS] Internet Aktif! Menghubungkan ke VPS...");
      if (clientVPS.connect(VPS_CLIENT_ID, VPS_USER, VPS_PASS)) {
        Serial.println(" TERHUBUNG!");
        clientVPS.subscribe(TOPIC_VPS_SUB_INTERVAL);
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
      Serial.print("[CLOUD] Internet Aktif! Menghubungkan ke Cloud Gateway...");
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
// INITIAL SETUP PERANGKAT
// =========================================================================
void setup() {
  Serial.begin(115200);
  setup_wifi();
  
  // Bypass SSL Verification
  espClientVPS.setInsecure(); 
  espClientCloud.setInsecure();

  // Inisialisasi Broker
  clientVPS.setServer(VPS_HOST, VPS_PORT);
  clientVPS.setCallback(callbackVPS);

  clientCloud.setServer(CLOUD_HOST, CLOUD_PORT);

  clientLocal.setServer(LOCAL_HOST, LOCAL_PORT);
  clientLocal.setCallback(callbackLocal);

  // Inisialisasi Bus I2C Custom & Sensor BME280
  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    Serial.println("[CRITICAL] Sensor BME280 tidak terdeteksi pada bus I2C!");
    while (1) delay(10);
  }
  Serial.println("[SUCCESS] Sensor BME280 Siap Digunakan!");
}

// =========================================================================
// LOOP UTAMA AKUISISI & TRANSMISI DATA
// =========================================================================
void loop() {
  if (wifiMulti.run() != WL_CONNECTED) setup_wifi();

  // Menjaga Koneksi 3 Server secara Non-Blocking
  maintainLocalConnection(); // Diprioritaskan lebih dulu (LAN Direct IP)
  maintainVPSConnection();   // Smart DNS check tiap 60 detik
  maintainCloudConnection(); // Smart DNS check tiap 60 detik

  if (clientLocal.connected()) clientLocal.loop();
  if (clientVPS.connected())   clientVPS.loop();
  if (clientCloud.connected()) clientCloud.loop();

  // Penjadwalan Pengiriman Data Berbasis Interval Dinamis
  if (millis() - timerPublish >= intervalPublishBaro) {
    timerPublish = millis();

    float tekanan_hPa = bme.readPressure() / 100.0F; 
    float suhu_c      = bme.readTemperature();

    if (!isnan(tekanan_hPa) && !isnan(suhu_c)) {
      String stringTemp = String(suhu_c, 2);
      String stringBaro = String(tekanan_hPa, 2);

      // -------------------------------------------------------------------
      // 1. PUBLISH KE SERVER LOCAL (Jalur Utama LAN Offline/Online)
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
      // 2. PUBLISH KE SERVER VPS (Hanya Jika Internet Aktif)
      // -------------------------------------------------------------------
      if (clientVPS.connected()) {
        StaticJsonDocument<256> docTemp;
        docTemp["label_lokasi"]     = LOKASI_ALAT;
        docTemp["label_nama Modul"] = "Modul_Baro_Temp";
        docTemp["label_macAddr"]    = MAC_ADDRESS;
        docTemp["data_kontrol"]     = stringTemp;
        docTemp["data_status"]      = "Normal";
        docTemp["data_berita"]      = "Data Suhu Kubah";
        
        String payloadTemp;
        serializeJson(docTemp, payloadTemp);
        clientVPS.publish(TOPIC_VPS_PUB_TEMP, payloadTemp.c_str());

        StaticJsonDocument<256> docBaro;
        docBaro["label_lokasi"]     = LOKASI_ALAT;
        docBaro["label_nama Modul"] = "Modul_Baro_Pres";
        docBaro["label_macAddr"]    = MAC_ADDRESS;
        docBaro["data_kontrol"]     = stringBaro;
        docBaro["data_status"]      = "Normal";
        docBaro["data_berita"]      = "Data Tekanan Barometrik";

        String payloadBaro;
        serializeJson(docBaro, payloadBaro);
        clientVPS.publish(TOPIC_VPS_PUB_BARO, payloadBaro.c_str());

        Serial.println("[VPS PUB] Data Terkirim ke VPS.");
      }

      // -------------------------------------------------------------------
      // 3. PUBLISH KE SERVER CLOUD GATEWAY (Hanya Jika Internet Aktif)
      // -------------------------------------------------------------------
      if (clientCloud.connected()) {
        StaticJsonDocument<512> docCloud;
        String keyBaro = "SENSOR_BARO_" + String(LOKASI_ALAT) + "_" + String(MAC_ADDRESS);
        
        JsonArray dataArray = docCloud.createNestedArray(keyBaro);
        
        JsonObject objTemp = dataArray.createNestedObject();
        objTemp["label_lokasi"]     = LOKASI_ALAT;
        objTemp["label_nama Modul"] = "Modul_Baro_Temp";
        objTemp["label_macAddr"]    = MAC_ADDRESS;
        objTemp["data_kontrol"]     = stringTemp;
        objTemp["data_status"]      = "Normal";
        objTemp["data_berita"]      = "Data Suhu Kubah";

        JsonObject objPres = dataArray.createNestedObject();
        objPres["label_lokasi"]     = LOKASI_ALAT;
        objPres["label_nama Modul"] = "Modul_Baro_Pres";
        objPres["label_macAddr"]    = MAC_ADDRESS;
        objPres["data_kontrol"]     = stringBaro;
        objPres["data_status"]      = "Normal";
        objPres["data_berita"]      = "Data Tekanan Barometrik";

        String payloadCloud;
        serializeJson(docCloud, payloadCloud);
        clientCloud.publish(TOPIC_CLOUD_GATEWAY, payloadCloud.c_str());

        Serial.println("[CLOUD GATEWAY PUB] Data Agregat Terkirim.");
      }

      Serial.println();
    }
  }
}