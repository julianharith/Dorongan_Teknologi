#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h> // Library untuk menyimpan data ke memori permanen

#define TRIGGER_PIN 0 

#ifndef RGB_BUILTIN
  #define RGB_BUILTIN 48 
#endif

// --- IDENTITAS ALAT & VERSI ---
const String SERIAL_NUMBER = "0001";
uint32_t currentVersion; // Dideklarasikan saja, nilainya diambil dari memori di setup()

// --- KONFIGURASI MQTT VPS ---
constexpr char MQTT_HOST[] = "hivemqtt.e-farmingcorpora.cloud";
constexpr uint16_t MQTT_PORT = 8883;
constexpr char MQTT_USER[] = "efarming";
constexpr char MQTT_PASSWORD[] = "EfarmingHiveMQ2026!";
constexpr char topic_cmd[] = "farm-da27b11b/sensor_7in1/firmware_update";  

WiFiManager wifiManager;
WiFiClientSecure mqttClient; 
PubSubClient client(mqttClient);
Preferences preferences; // Inisialisasi memori permanen

bool perintahUpdateDiterima = false;
String urlUpdateDinamis = ""; 
uint32_t versiUpdateDinamis = 0; // Menyimpan sementara versi target dari JSON

// --- FUNGSI KONTROL LED RGB ---
void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}

// --- CALLBACK WIFIMANAGER ---
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("\nGagal terkoneksi ke router. Membuka Portal AP...");
  setStatusLED(0, 0, 255); // Biru
}

// --- FUNGSI EKSEKUSI OTA ---
void performOTA(String url, uint32_t newVersion) {
  Serial.print("\nMemulai unduhan OTA dari: ");
  Serial.println(url);
  
  setStatusLED(0, 255, 255); // Cyan: Proses OTA

  WiFiClientSecure secureClient;
  secureClient.setInsecure(); 

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  if (http.begin(secureClient, url)) {
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      int contentLength = http.getSize();
      bool canBegin = Update.begin(contentLength);

      if (canBegin) {
        Serial.println("Mengunduh dan menulis file ke memori...");
        WiFiClient* stream = http.getStreamPtr();
        size_t written = Update.writeStream(*stream);

        if (written == contentLength) {
          Serial.println("File berhasil diunduh secara penuh!");
        }

        // --- VALIDASI AKHIR DAN SIMPAN VERSI BARU ---
        if (Update.end()) {
          Serial.println("OTA Selesai! Menyimpan versi baru ke memori permanen...");
          
          // Simpan angka target_version dari JSON agar digunakan saat alat menyala lagi
          preferences.putUInt("fw_version", newVersion); 
          
          setStatusLED(0, 255, 0); 
          delay(1000);
          ESP.restart();
        } else {
          Serial.printf("Update Error: %s\n", Update.errorString());
          setStatusLED(255, 0, 0); 
          delay(2000);
        }
      } else {
         Serial.println("Gagal: Kapasitas partisi OTA tidak mencukupi.");
         setStatusLED(255, 0, 0);
         delay(2000);
      }
    } else {
      Serial.printf("Gagal mengunduh firmware, HTTP Error: %d\n", httpCode);
      setStatusLED(255, 0, 0);
      delay(2000);
    }
    http.end();
  } else {
    Serial.println("Gagal terhubung ke server target.");
    setStatusLED(255, 0, 0);
    delay(2000);
  }
  
  perintahUpdateDiterima = false; 
}

// --- CALLBACK MQTT (Membaca JSON) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("\nPesan diterima di topik [%s]\n", topic);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("Format pesan bukan JSON yang valid: ");
    Serial.println(error.c_str());
    return;
  }

  bool update_flag = doc["update_flag"] | false;
  uint32_t target_version = doc["version"] | 0;
  String target_sn = doc["serial_number"] | "";
  String target_url = doc["url"] | "";

  if (update_flag) {
    Serial.println("Mengecek validitas perangkat...");
    Serial.printf("SN Alat: %s | SN Target: %s\n", SERIAL_NUMBER.c_str(), target_sn.c_str());

    if (target_sn == SERIAL_NUMBER) {
      
      if (target_version > currentVersion) {
        Serial.println("Otentikasi berhasil! Menjadwalkan OTA...");
        
        // Simpan ke variabel global sementara
        urlUpdateDinamis = target_url; 
        versiUpdateDinamis = target_version; 
        perintahUpdateDiterima = true; 
        
      } else {
        Serial.println("Diabaikan: Versi firmware target tidak lebih baru.");
      }
      
    } else {
      Serial.println("Diabaikan: Nomor Seri tidak cocok dengan alat ini.");
    }
  }
}

// --- FUNGSI KONEKSI ULANG MQTT ---
void reconnectMQTT() {
  if (!client.connected() && !perintahUpdateDiterima) {
    setStatusLED(0, 255, 0); // Hijau

    Serial.print("Mencoba terhubung ke broker MQTT SSL...");
    String clientId = "ESP32-DorTek-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Berhasil terhubung ke MQTT!");
      client.subscribe(topic_cmd); 
      
      setStatusLED(255, 0, 255); // Ungu
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(". Coba lagi dalam 5 detik.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  // --- INISIALISASI MEMORI PERMANEN ---
  preferences.begin("sistem", false); // Buka ruang penyimpanan bernama "sistem"
  
  // Ambil versi dari memori. Jika memori kosong (pertama kali flash), gunakan 2026091500
  currentVersion = preferences.getUInt("fw_version", 2026091500); 

  Serial.println("\n--- MEMULAI SISTEM ---");
  Serial.printf("Versi Saat Ini: %u\n", currentVersion);
  Serial.printf("Nomor Seri: %s\n", SERIAL_NUMBER.c_str());

  setStatusLED(255, 150, 0); // Kuning

  wifiManager.setConnectTimeout(30);       
  wifiManager.setConfigPortalTimeout(180); 
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("Setup-Sensor-Tanah")) {
    Serial.println("Gagal terhubung ke WiFi & timeout habis. Restarting...");
    setStatusLED(255, 0, 0); 
    delay(3000);
    ESP.restart();
  }

  Serial.println("\n--- TERHUBUNG KE WIFI ---");
  setStatusLED(0, 255, 0); 

  mqttClient.setInsecure(); 
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void loop() {
  // 1. Eksekusi OTA 
  if (perintahUpdateDiterima) {
    Serial.println("Memutus MQTT sementara untuk alokasi RAM proses HTTPS OTA...");
    client.disconnect(); 
    delay(500);

    // Kirimkan URL dan Versi Target ke fungsi OTA
    performOTA(urlUpdateDinamis, versiUpdateDinamis); 
  }

  // 2. Pertahankan Koneksi MQTT
  if (!client.connected() && !perintahUpdateDiterima) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      reconnectMQTT();
    }
  } else if (client.connected()) {
    client.loop();
  }

  // 3. Manual Reset WiFi via Tombol BOOT
  if (digitalRead(TRIGGER_PIN) == LOW) {
    delay(3000); 
    if (digitalRead(TRIGGER_PIN) == LOW) {
      Serial.println("\nTombol BOOT ditahan! Membuka portal setup...");
      setStatusLED(0, 0, 255); 
      wifiManager.setConfigPortalTimeout(180); 
      if (!wifiManager.startConfigPortal("Setup-Sensor-Tanah")) {
        setStatusLED(255, 0, 0);
        delay(2000);
        ESP.restart();
      }
      setStatusLED(0, 255, 0); 
    }
  }
}