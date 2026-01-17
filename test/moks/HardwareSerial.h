#ifndef TEST_MOCKS_HARDWARESERIAL_H
#define TEST_MOCKS_HARDWARESERIAL_H

#include <cstddef>
#include <cstdint>
#include <functional>
class HardwareSerial {
public:
    void begin(unsigned long, ...) {}
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t*, size_t size) { return size; }
    int available() { return 0; }
    int read() { return -1; }
    void flush() {}
    int peek() { return -1; }
    void onReceive(std::function<void()> callback) {}
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;

#endif