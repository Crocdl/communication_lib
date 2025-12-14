#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ipc {

using byte_t    = uint8_t;
using time_ms_t = uint32_t;

enum class Status : uint8_t {
    Ok = 0,
    Busy,
    Timeout,
    CrcError,
    FrameError,
    BufferOverflow,
    InvalidArgument
};

enum class PacketType : uint8_t {
    Data = 0x01,
    Ack  = 0x02,
    Nack = 0x03,
    Ping = 0x04,
    Pong = 0x05
};

} // namespace ipc
