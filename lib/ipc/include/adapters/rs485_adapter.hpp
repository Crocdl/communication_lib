#ifndef IPC_RS485_ADAPTER_HPP
#define IPC_RS485_ADAPTER_HPP

#include "../transport_adapter.hpp"

namespace ipc {

class RS485Adapter : public ITransportAdapter {
public:
    RS485Adapter() noexcept;
    size_t send(const byte*, size_t) noexcept override;
    void set_rx_handler(RxHandler, void*) noexcept override;
};

} // namespace ipc

#endif // IPC_RS485_ADAPTER_HPP