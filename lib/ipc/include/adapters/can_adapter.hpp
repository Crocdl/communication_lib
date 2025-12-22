#ifndef IPC_CAN_ADAPTER_HPP
#define IPC_CAN_ADAPTER_HPP

#include "../transport_adapter.hpp"

namespace ipc {

class CANAdapter : public ITransportAdapter {
public:
    CANAdapter() noexcept;
    size_t send(const byte*, size_t) noexcept override;
    void set_rx_handler(RxHandler, void*) noexcept override;
};

} // namespace ipc

#endif // IPC_CAN_ADAPTER_HPP
