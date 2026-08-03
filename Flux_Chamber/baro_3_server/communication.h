#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

extern PubSubClient clientVPS;
extern PubSubClient clientLocal;

void setupCommunication();
void maintainCommunication();
void publishBaroData(float suhu, float tekanan);