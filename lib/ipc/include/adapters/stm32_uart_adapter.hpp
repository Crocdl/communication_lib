#ifndef IPC_STM32_UART_ADAPTER_HPP
#define IPC_STM32_UART_ADAPTER_HPP

#include "../transport_adapter.hpp"
#include <cstdint>
#include <cstddef>

#if defined(USE_HAL_DRIVER) || defined(STM32) || defined(STM32G4)
#include "main.h"
#else
#include "mock_adapters/mock_uart_adapter.hpp"
#endif

namespace ipc {

class STM32UARTAdapter : public ITransportAdapter {
public:
    struct Config {
        uint32_t baudrate;
        bool register_hal_callbacks;
        bool buffer_rx_in_interrupt;
        uint32_t tx_timeout_ms;

        Config() noexcept
            : baudrate(115200),
              register_hal_callbacks(false),
              buffer_rx_in_interrupt(true),
              tx_timeout_ms(10) {}
    };

    explicit STM32UARTAdapter(UART_HandleTypeDef* huart) noexcept;
    ~STM32UARTAdapter() noexcept;

    bool init(const Config& config) noexcept;

    size_t send(const byte* data, size_t len) noexcept override;
    void set_rx_handler(RxHandler handler, void* ctx) noexcept override;
    void poll() noexcept override;

    uint32_t uart_errors() const noexcept { return uart_errors_; }
    uint32_t rx_overruns() const noexcept { return rx_overruns_; }
    bool tx_busy() const noexcept { return tx_busy_; }

    static void handle_rx_complete(UART_HandleTypeDef* huart) noexcept;
    static void handle_tx_complete(UART_HandleTypeDef* huart) noexcept;
    static void handle_error(UART_HandleTypeDef* huart) noexcept;

private:
    static constexpr size_t kMaxInstances = 4;
    static constexpr size_t kRxBufferSize = 256;
    static constexpr size_t kTxBufferSize = 512;

    UART_HandleTypeDef* huart_;
    Config config_;
    RxHandler rx_handler_;
    void* rx_context_;

    byte rx_byte_;
    byte rx_buffer_[kRxBufferSize];
    volatile size_t rx_head_;
    volatile size_t rx_tail_;

    byte tx_buffer_[kTxBufferSize];
    volatile bool tx_busy_;
    bool initialized_;

    uint32_t uart_errors_;
    uint32_t rx_overruns_;

    bool register_instance_() noexcept;
    void unregister_instance_() noexcept;
    bool start_rx_() noexcept;
    void on_rx_complete_() noexcept;
    void on_tx_complete_() noexcept;
    void on_error_() noexcept;
    void push_rx_byte_(byte b) noexcept;
    bool pop_rx_byte_(byte* b) noexcept;

    static STM32UARTAdapter* find_instance_(UART_HandleTypeDef* huart) noexcept;
    static STM32UARTAdapter* instances_[kMaxInstances];
};

} // namespace ipc

#endif // IPC_STM32_UART_ADAPTER_HPP
