#ifndef IPC_CAN_ADAPTER_HPP
#define IPC_CAN_ADAPTER_HPP

#include "../transport_adapter.hpp"
#include <cstdint>

// STM32 HAL include
#ifdef STM32G4
#include "main.h"
#endif

namespace ipc {

class CANAdapter : public ITransportAdapter {
public:
    enum class BaudRate : uint32_t {
        kBaud500k = 500000,
        kBaud1M = 1000000,
        kBaud2M = 2000000,
        kBaud4M = 4000000,
        kBaud5M = 5000000,
    };

    // CAN FD Frame configuration
    struct CanFdConfig {
        BaudRate nominal_bitrate;      // Nominal (Arbitration) bitrate
        BaudRate data_bitrate;         // Data phase bitrate (for FD frames)
        bool enable_fd;                // Enable CAN FD support
        bool enable_brs;               // Enable Bit Rate Switching for FD
        bool enable_esi;               // Enable Error State Indicator for FD
        uint32_t std_filter_mask;      // Standard ID filter mask
        uint32_t ext_filter_mask;      // Extended ID filter mask
    };

    explicit CANAdapter(CAN_HandleTypeDef* hcan) noexcept;
    ~CANAdapter() noexcept;

    // Initialize CAN with FD configuration
    bool init(const CanFdConfig& config) noexcept;

    // ITransportAdapter implementation
    size_t send(const byte* data, size_t len) noexcept override;
    void set_rx_handler(RxHandler handler, void* ctx) noexcept override;
    void poll() noexcept override;

    // CAN-specific methods
    bool send_can_message(uint32_t can_id, const byte* data, size_t len,
                          bool is_extended = false, bool is_fd = false) noexcept;

    // Static interrupt handlers for HAL callbacks
    static void rx_fifo0_callback(CAN_HandleTypeDef* hcan);
    static void rx_fifo1_callback(CAN_HandleTypeDef* hcan);
    static void error_callback(CAN_HandleTypeDef* hcan);

private:
    static constexpr size_t kMaxFrameLength = 64;  // Max CAN FD frame payload
    static constexpr size_t kMaxRxMessages = 32;   // Max buffered RX messages
    
    struct RxMessage {
        uint32_t can_id;
        uint8_t dlc;
        uint8_t data[kMaxFrameLength];
        bool is_fd;
        bool is_extended;
    };

    CAN_HandleTypeDef* hcan_;
    RxHandler rx_handler_;
    void* rx_context_;
    
    RxMessage rx_buffer_[kMaxRxMessages];
    size_t rx_head_;
    size_t rx_tail_;
    
    bool is_initialized_;

    // Helper methods
    void setup_filters_() noexcept;
    void configure_bitrate_(BaudRate nominal, BaudRate data) noexcept;
    bool is_rx_buffer_full_() const noexcept;
    bool is_rx_buffer_empty_() const noexcept;
    void push_rx_message_(const RxMessage& msg) noexcept;
    bool pop_rx_message_(RxMessage& msg) noexcept;
    
    // Interrupt handler delegation
    void handle_rx_fifo0_() noexcept;
    void handle_rx_fifo1_() noexcept;
    void handle_error_() noexcept;

    // Instance pointer for static callbacks
    static CANAdapter* instance_;
};

} // namespace ipc

#endif // IPC_CAN_ADAPTER_HPP
