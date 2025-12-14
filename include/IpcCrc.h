#pragma once
#include "IpcTypes.h"

namespace ipc {

// CRC16 (IBM / Modbus)
struct Crc16 {
    static uint16_t calculate(const byte_t* data, size_t length);
};

// Политика без CRC (для отладки / тестов)
struct NoCrc {
    static uint16_t calculate(const byte_t*, size_t) {
        return 0;
    }
};

} // namespace ipc
