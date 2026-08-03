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

WiFiClientSecure espClientCloud;
PubSubClient clientCloud(espClientCloud);

unsigned long lastLocalTryTimer = 0;
unsigned long lastCloudCheckTimer = 0;

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

void setupCommunication() {
  setup_wifi(); 
  espClientVPS.setInsecure(); 
  espClientCloud.setInsecure();

  clientVPS.setServer(VPS_HOST, VPS_PORT);
  clientVPS.setCallback(callbackVPS);

  clientCloud.setServer(CLOUD_HOST, CLOUD_PORT);

  clientLocal.setServer(LOCAL_HOST, LOCAL_PORT);
  clientLocal.setCallback(callbackLocal);
}

void maintainCommunication() {
  if (wifiMulti.run() != WL_CONNECTED) setup_wifi();
  maintainLocalConnection();
  if (clientLocal.connected()) clientLocal.loop();
  if (clientVPS.connected())   clientVPS.loop();
  if (clientCloud.connected()) clientCloud.loop();
}

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