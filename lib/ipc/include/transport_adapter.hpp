#ifndef IPC_TRANSPORT_ADAPTER_HPP
#define IPC_TRANSPORT_ADAPTER_HPP

#include <cstdint>
#include <cstddef>

namespace ipc {

using byte = uint8_t;

class ITransportAdapter {
public:
    using RxHandler = void(*)(byte, void*);

    virtual ~ITransportAdapter() = default;
    virtual size_t send(const byte* data, size_t len) noexcept = 0;
    virtual void set_rx_handler(RxHandler handler, void* ctx) noexcept = 0;
    virtual void poll() noexcept {}
};

} // namespace ipc

#endif // IPC_TRANSPORT_ADAPTER_HPP