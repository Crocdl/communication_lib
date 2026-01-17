#include <Arduino.h>

// Включаем вашу библиотеку
#include "../lib/ipc/include/ipc.hpp"

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("ESP32 IPC Library Test");
    
    // Здесь можно протестировать вашу библиотеку
    // Например:
    // IPC::initialize();
    // или создать экземпляр какого-то класса
}

void loop() {
    // Основной цикл
    delay(1000);
    Serial.println("Working...");
}