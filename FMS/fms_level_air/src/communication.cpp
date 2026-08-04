#include "communication.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>

WiFiMulti wifiMulti;

WiFiClientSecure espClientVPS;
PubSubClient clientVPS(espClientVPS);

unsigned long lastCloudCheckTimer = 0;

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

void setupCommunication() {
  setup_wifi(); 
  
  // Set mode pengamanan SSL/TLS persis seperti kode pengujian Anda
  espClientVPS.setInsecure(); 
  
  // SEMUA BATASAN TIMEOUT 5 DETIK DIHAPUS
  // Kita biarkan ESP32 menggunakan pengaturan defaultnya yang terbukti berhasil

  clientVPS.setServer(VPS_HOST, VPS_PORT);
  
  // Ukuran buffer tetap kita besarkan agar cukup untuk menampung teks JSON yang panjang
  clientVPS.setBufferSize(1024); 
}

void maintainCommunication() {
  // Jaga koneksi Wi-Fi
  if (wifiMulti.run() != WL_CONNECTED) {
    setup_wifi();
  }
  
  // Jaga koneksi MQTT VPS
  if (WiFi.status() == WL_CONNECTED) {
    if (!clientVPS.connected()) {
      // Jeda percobaan diperpanjang menjadi 10 detik agar tidak terjadi penumpukan memori
      if (millis() - lastCloudCheckTimer >= 10000) {
        lastCloudCheckTimer = millis();
        
        Serial.print("[VPS] Menghubungkan ke broker MQTT... ");
        
        // Membuat Client ID acak
        String clientId = "ESP32Master-";
        clientId += String(random(0xffff), HEX);
        
        if (clientVPS.connect(clientId.c_str(), VPS_USER, VPS_PASS)) {
          Serial.println("BERHASIL!");
        } else {
          Serial.print("GAGAL, rc=");
          Serial.println(clientVPS.state());
        }
      }
    } else {
      // Pertahankan koneksi
      clientVPS.loop();
    }
  }
}

void publishJSONToVPSAndLocal(const char* topic, const char* nama_modul, String variabel, String satuan, String data_kontrol, String data_status, String data_berita) {
  
  // Hanya buat dan kirim JSON jika MQTT tersambung
  if (!clientVPS.connected()) {
    return;
  }

  // --- FORMAT JSON VPS (NESTED) ---
  StaticJsonDocument<768> docVPS;
  String rootKey = "sensorNode_farmda27b11b_" + String(MAC_ADDRESS);
  
  JsonArray dataArray = docVPS.createNestedArray(rootKey);
  JsonObject innerObj = dataArray.createNestedObject();

  innerObj["label_lokasi"]     = "farmda27b11b"; 
  innerObj["label_nama_Modul"] = nama_modul;   
  innerObj["label_variabel"]   = variabel;
  innerObj["label_satuan"]     = satuan;
  innerObj["label_macAddr"]    = MAC_ADDRESS;  
  innerObj["data_kontrol"]     = data_kontrol.toFloat(); 
  innerObj["data_status"]      = data_status;
  innerObj["data_berita"]      = data_berita;

  String payloadVPS;
  serializeJson(docVPS, payloadVPS);

  // Eksekusi Publish
  clientVPS.publish(topic, payloadVPS.c_str());
}