#include "valve_control.h"
#include "config.h"
#include "communication.h"

void initValveHardware() {
  pinMode(pinBuka, OUTPUT);
  pinMode(pinTutup, OUTPUT);
  pinMode(pinVacuum, OUTPUT);
  
  digitalWrite(pinBuka, LOW);
  digitalWrite(pinTutup, LOW);
  digitalWrite(pinVacuum, LOW);
  
  Serial.println("[SUCCESS] Driver Relay Katup & Pompa Siap.");
}

void executeValveCommand(int commandCode) {
  if (commandCode == 1) { 
    Serial.println("[MECHANICAL] Menjalankan Aktuator: PROSES MENUTUP KATUP...");
    digitalWrite(pinVacuum, LOW);    
    digitalWrite(pinBuka, LOW);      
    digitalWrite(pinTutup, HIGH);    
    delay(7500);                     
    digitalWrite(pinTutup, LOW);     
    
    publishStatusAllServers("1", "OK", "Kubah Tertutup Rapat. Fase Sampling Dimulai.");
    Serial.println("[VALVE REPORT] Status 1 (Tertutup) Terkirim.");
  } 
  else if (commandCode == 0) {
    Serial.println("[MECHANICAL] Menjalankan Aktuator: PROSES MEMBUKA KATUP & VAKUM ON...");
    digitalWrite(pinTutup, LOW);     
    digitalWrite(pinBuka, HIGH);     
    delay(7000);                     
    digitalWrite(pinBuka, LOW);      
    
    digitalWrite(pinVacuum, HIGH);   
    
    publishStatusAllServers("0", "OK", "Katup Terbuka Lebar. Pompa Vakum Aktif Membersihkan.");
    Serial.println("[VALVE REPORT] Status 0 (Terbuka) Terkirim.");
  }
}