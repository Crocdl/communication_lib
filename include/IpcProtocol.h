#pragma once
#include "IpcTypes.h"

namespace ipc {

// Максимальные размеры (подбираются под МК)
constexpr size_t IPC_MAX_PAYLOAD = 64;
constexpr size_t IPC_FRAME_MAX   = 96;

// Тип пакета
enum class PacketType : uint8_t {
    Data = 0x01,
    Ack  = 0x02,
    Nack = 0x03,
    Ping = 0x04,
    Pong = 0x05
};

// Заголовок пакета (до фрейминга)
struct PacketHeader {
    uint8_t     type;
    uint8_t     seq;
    uint16_t    length;
};

// Полный пакет (логический)
struct Packet {
    PacketHeader header;
    uint8_t      payload[IPC_MAX_PAYLOAD];
    uint16_t     crc;
};

}
