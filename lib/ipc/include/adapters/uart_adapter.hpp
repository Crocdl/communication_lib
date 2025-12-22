#ifndef IPC_UART_ADAPTER_HPP
#define IPC_UART_ADAPTER_HPP

#include "../transport_adapter.hpp"

namespace ipc {

class UARTAdapter : public ITransportAdapter {
public:
    UARTAdapter() noexcept;
    size_t send(const byte*, size_t) noexcept override;
    void set_rx_handler(RxHandler, void*) noexcept override;
    void poll() noexcept override;
    void isr_rx_byte(byte b) noexcept;
private:
    RxHandler handler_ = nullptr;
    void* ctx_ = nullptr;
};

} // namespace ipc

#endif // IPC_UART_ADAPTER_HPP