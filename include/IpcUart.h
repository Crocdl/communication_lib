#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ipc {

using byte = uint8_t;
using time_ms_t = uint32_t;

enum class Status : uint8_t {
    Ok,
    Timeout,
    CrcError,
    FrameError,
    BufferOverflow,
    NotReady
};

}
