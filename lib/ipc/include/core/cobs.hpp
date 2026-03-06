#ifndef IPC_COBS_HPP
#define IPC_COBS_HPP

#include <cstdint>
#include <cstddef>

namespace ipc {

using byte = uint8_t;

size_t cobs_encode(const byte* in, size_t in_len, byte* out, size_t out_size) noexcept;
size_t cobs_decode(const byte* in, size_t in_len, byte* out, size_t out_size) noexcept;

} // namespace ipc

#endif // IPC_COBS_HPP