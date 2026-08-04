#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

extern PubSubClient clientVPS;
extern PubSubClient clientLocal;

void setupCommunication();
void maintainCommunication();

// Parameter topik dikembalikan menjadi satu (const char* topic)
void publishJSONToVPSAndLocal(const char* topic, const char* nama_modul, String variabel, String satuan, String data_kontrol, String data_status, String data_berita);