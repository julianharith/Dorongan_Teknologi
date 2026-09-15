#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h> 

#define TRIGGER_PIN 0 

#ifndef RGB_BUILTIN
  #define RGB_BUILTIN 48 
#endif

// --- KONFIGURASI GITHUB OTA ---
const char* binUrl = "https://github.com/julianharith/Dorongan_Teknologi/releases/latest/download/firmware.bin";

// --- KONFIGURASI MQTT VPS ---
constexpr char MQTT_HOST[] = "hivemqtt.e-farmingcorpora.cloud";
constexpr uint16_t MQTT_PORT = 8883;
constexpr char MQTT_USER[] = "efarming";
constexpr char MQTT_PASSWORD[] = "EfarmingHiveMQ2026!";
constexpr char topic_cmd[] = "farm-da27b11b/sensor_7in1/firmware_update";  

WiFiManager wifiManager;
WiFiClientSecure mqttClient; // Diubah menjadi WiFiClientSecure untuk port 8883 (SSL/TLS)
PubSubClient client(mqttClient);

bool perintahUpdateDiterima = false;

// --- FUNGSI KONTROL LED RGB ---
void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_BUILTIN, r, g, b);
}

// --- CALLBACK WIFIMANAGER ---
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("\nGagal terkoneksi ke router. Membuka Portal AP...");
  setStatusLED(0, 0, 255); // Biru: Mode AP
}

// --- FUNGSI EKSEKUSI OTA ---
void performOTA() {
  Serial.print("\nMemulai unduhan OTA dari: ");
  Serial.println(binUrl);
  
  setStatusLED(0, 255, 255); // Cyan: Proses OTA

  WiFiClientSecure secureClient;
  secureClient.setInsecure(); 

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  if (http.begin(secureClient, binUrl)) {
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
        } else {
          Serial.printf("Peringatan: Terunduh %d dari %d bytes\n", written, contentLength);
        }

        if (Update.end()) {
          Serial.println("OTA Selesai! Sistem akan restart dalam 1 detik...");
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
    Serial.println("Gagal terhubung ke server GitHub.");
    setStatusLED(255, 0, 0);
    delay(2000);
  }
  
  perintahUpdateDiterima = false; 
}

// --- CALLBACK MQTT (Menerima Pesan) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String pesan = "";
  for (int i = 0; i < length; i++) {
    pesan += (char)payload[i];
  }
  
  Serial.printf("Pesan diterima di topik [%s]: %s\n", topic, pesan.c_str());

  if (pesan == "UPDATE_OTA") {
    Serial.println("Perintah UPDATE diterima! Menjadwalkan OTA...");
    perintahUpdateDiterima = true; 
  }
}

// --- FUNGSI KONEKSI ULANG MQTT ---
void reconnectMQTT() {
  if (!client.connected() && !perintahUpdateDiterima) {
    setStatusLED(0, 255, 0); // Hijau: WiFi terhubung, MQTT terputus/menyambung

    Serial.print("Mencoba terhubung ke broker MQTT SSL...");
    String clientId = "ESP32-DorTek-" + String(random(0xffff), HEX);
    
    // Autentikasi dengan Username dan Password
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Berhasil terhubung ke MQTT!");
      client.subscribe(topic_cmd); 
      setStatusLED(255, 0, 255); // Ungu: MQTT berhasil terhubung
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

  Serial.println("\n--- MEMULAI SISTEM VERSI 1.1 (MQTT OTA TEST) ---");

  setStatusLED(255, 150, 0); // Kuning: Mulai WiFi setup

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

  // Setup MQTT Secure
  mqttClient.setInsecure(); // Abaikan sertifikat CA agar stabil di berbagai broker
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void loop() {
  // 1. Eksekusi OTA jika perintah diterima
  if (perintahUpdateDiterima) {
    Serial.println("Memutus MQTT sementara untuk alokasi RAM proses HTTPS OTA...");
    client.disconnect(); 
    delay(500);

    performOTA(); 
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