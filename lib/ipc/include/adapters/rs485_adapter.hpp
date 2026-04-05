#ifndef IPC_RS485_ADAPTER_HPP
#define IPC_RS485_ADAPTER_HPP

#include "../transport_adapter.hpp"
#include <cstdint>

// STM32 HAL include
#ifdef STM32G4
#include "main.h"
#else 
#include "mock_adapters/mock_uart_adapter.hpp"
#endif

namespace ipc {

class RS485Adapter : public ITransportAdapter {
public:
    // DE/RE control modes
    enum class DEMode : uint8_t {
        kManual = 0,        // Manual control via set_de_state
        kAutomatic = 1,     // Automatic control (DE during TX, low during RX)
    };

    struct Config {
        uint32_t baudrate;          // Serial baudrate (9600, 19200, 115200, etc.)
        DEMode de_mode;             // Driver Enable mode
        uint32_t de_assert_delay;   // Delay after DE assertion before TX (µs)
        uint32_t de_deassert_delay; // Delay after TX before DE deassertion (µs)
    };

    // Constructor for STM32 HAL
    explicit RS485Adapter(UART_HandleTypeDef* huart) noexcept;
    
    // Manual DE control (only if de_mode = kManual)
    void set_de_state(bool active) noexcept;

    // Initialize RS485 with configuration
    bool init(const Config& config) noexcept;

    // ITransportAdapter implementation
    size_t send(const byte* data, size_t len) noexcept override;
    void set_rx_handler(RxHandler handler, void* ctx) noexcept override;
    void poll() noexcept override;

private:
    static constexpr size_t kMaxRxBuffer = 256;
    
    UART_HandleTypeDef* huart_;
    RxHandler rx_handler_;
    void* rx_context_;
    Config config_;
    
    uint8_t rx_buffer_[kMaxRxBuffer];
    size_t rx_idx_;
    
    bool is_initialized_;
    bool de_state_;  // Current state of DE line

    // Helper methods
    void enable_rx_() noexcept;
    void disable_rx_() noexcept;
    void assert_de_() noexcept;
    void deassert_de_() noexcept;
    
    // HAL callbacks
    static void rx_callback_(UART_HandleTypeDef* huart);
    static void tx_complete_callback_(UART_HandleTypeDef* huart);
    static void error_callback_(UART_HandleTypeDef* huart);

    // Instance pointer for static callbacks
    static RS485Adapter* instance_;
};

} // namespace ipc

#endif // IPC_RS485_ADAPTER_HPP