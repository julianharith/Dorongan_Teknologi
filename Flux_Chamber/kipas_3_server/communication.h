#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

extern PubSubClient clientVPS;
extern PubSubClient clientLocal;

void setupCommunication();
void maintainCommunication();
void publishStatusAllServers(String data_kontrol, String data_status, String data_berita);