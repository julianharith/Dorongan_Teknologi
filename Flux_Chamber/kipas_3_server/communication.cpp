#include "communication.h"
#include "config.h"
#include "fan_control.h"
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

volatile bool isVPSConnecting = false;

void publishStatusAllServers(String data_kontrol, String data_status, String data_berita) {
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
    clientLocal.publish(TOPIC_LOCAL_STS_FAN, payloadLocal.c_str());
    Serial.printf("[LOCAL STS PUB] -> Terkirim\n");
  }

  if (clientVPS.connected() && !isVPSConnecting) {
    StaticJsonDocument<768> docVPS;
    String rootKey = "sensorNode_farmda27b11b_" + String(MAC_ADDRESS);
    
    JsonArray dataArray = docVPS.createNestedArray(rootKey);
    JsonObject innerObj = dataArray.createNestedObject();
    
    innerObj["label_lokasi"]     = "farmda27b11b";
    innerObj["label_nama_Modul"] = "sensor_co2"; 
    innerObj["label_variabel"]   = "fan_pwm";
    innerObj["label_satuan"]     = "%";
    innerObj["label_macAddr"]    = MAC_ADDRESS;  
    innerObj["data_kontrol"]     = data_kontrol.toInt(); 
    innerObj["data_status"]      = "normal";  
    innerObj["data_berita"]      = data_berita;  

    String payloadVPS;
    serializeJson(docVPS, payloadVPS);
    
    clientVPS.publish("farmda27b11b", payloadVPS.c_str());
    delay(10); clientVPS.loop();
    clientVPS.publish(TOPIC_VPS_STS_FAN, payloadVPS.c_str());
    Serial.printf("[VPS STS PUB] -> Terkirim (Nested Format)\n");
  }
}

void processIncomingCommand(String msg) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, msg);

  String commandValue = "";
  if (error) {
    commandValue = msg; 
  } else {
    if(doc["data_kontrol"].is<int>()) {
      commandValue = String(doc["data_kontrol"].as<int>());
    } else {
      commandValue = doc["data_kontrol"].as<String>(); 
    }
  }

  commandValue.trim();
  Serial.printf("\n[FAN RECV] Menerima Perintah Kecepatan: %s\n", commandValue.c_str());

  int targetSpeed = commandValue.toInt();
  executeFanCommand(targetSpeed);

  if (targetSpeed == 0) {
    publishStatusAllServers(String(targetSpeed), "OK", "Kipas Dimatikan (Daya 0%).");
  } else {
    String pesanBerita = "Kipas Berputar Aktif Berkecepatan PWM " + String(targetSpeed);
    publishStatusAllServers(String(targetSpeed), "OK", pesanBerita);
  }
}

void callbackVPS(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_VPS_CMD_FAN) processIncomingCommand(msg);
}

void callbackLocal(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  if (String(topic) == TOPIC_LOCAL_CMD_FAN) processIncomingCommand(msg);
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
}

void maintainLocalConnection() {
  if (clientLocal.connected()) return;

  if (millis() - lastLocalTryTimer >= 5000) {
    lastLocalTryTimer = millis();
    Serial.print("[LOCAL] Memeriksa Server Lokal (192.168.0.130)...");
    
    if (clientLocal.connect(LOCAL_CLIENT_ID, LOCAL_USER, LOCAL_PASS)) {
      Serial.println(" TERHUBUNG!");
      clientLocal.subscribe(TOPIC_LOCAL_CMD_FAN);
    } else {
      Serial.println(" OFF / Tidak Terjangkau.");
    }
  }
}

// =========================================================================
// FREERTOS TASK: BERJALAN PARALEL DI CORE 0
// =========================================================================
void vpsConnectionTask(void * parameter) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED && !clientVPS.connected()) {
      if (millis() - lastVPSTryTimer >= 10000) {
        lastVPSTryTimer = millis();
        isVPSConnecting = true;
        
        IPAddress dummyIP;
        if (WiFi.hostByName(VPS_HOST, dummyIP) == 1) {
          Serial.print("[VPS-TASK] Internet Aktif! Menghubungkan Kipas ke VPS...");
          if (clientVPS.connect(VPS_CLIENT_ID, VPS_USER, VPS_PASS)) {
            Serial.println(" TERHUBUNG!");
            clientVPS.subscribe(TOPIC_VPS_CMD_FAN); 
          } else {
            Serial.println(" GAGAL!");
          }
        } else {
          Serial.println("[VPS-TASK] No Internet. (VPS Skipped)");
        }
        isVPSConnecting = false;
      }
    }
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

  xTaskCreatePinnedToCore(
    vpsConnectionTask,
    "Task_VPS_Fan",
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
  
  if (clientVPS.connected() && !isVPSConnecting) clientVPS.loop();
}