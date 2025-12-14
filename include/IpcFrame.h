#pragma once
#include "IpcTypes.h"

namespace ipc {

#pragma pack(push, 1)
struct FrameHeader {
    uint8_t  type;
    uint8_t  seq;
    uint16_t length;
};
#pragma pack(pop)

template <size_t MaxPayload>
struct Frame {
    FrameHeader header;
    byte_t payload[MaxPayload];
    uint16_t crc;
};

} // namespace ipc
