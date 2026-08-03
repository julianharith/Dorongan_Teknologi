#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

extern PubSubClient clientVPS;
extern PubSubClient clientLocal;
// extern PubSubClient clientCloud; // CLOUD COMMENTED

void setupCommunication();
void maintainCommunication();
void checkAndConnectCloudServices(); 

// Deklarasi fungsi yang telah ditambah argumen "variabel" dan "satuan"
void publishJSONToVPSAndLocal(const char* topic, const char* nama_modul, String variabel, String satuan, String data_kontrol, String data_status, String data_berita);

// void publishGatewayCloud(String module_name, String data_ctrl, String status_msg, String news); // CLOUD COMMENTED