#ifndef IPC_UART_COMMUNICATION_HPP
#define IPC_UART_COMMUNICATION_HPP

#include "communication_layer.hpp"
#include "transport_adapter.hpp"
#include <cstdint>

#ifdef STM32G4
#include "main.h"
#endif

namespace ipc {

// ============================================================================
// UART Communication Layer (all devices independent, point-to-point or broadcast)
// ============================================================================

class UARTCommunication : public CommunicationLayer {
public:
    struct Config {
        uint8_t device_id;              // Device ID in network
        uint32_t baudrate;              // 9600, 115200, etc.
    };

    explicit UARTCommunication(ITransportAdapter* adapter) noexcept;
    ~UARTCommunication() noexcept;

    bool init(const Config& config) noexcept;

    // CommunicationLayer interface
    uint8_t get_device_id() const noexcept override;
    bool send_message(const Message& msg) noexcept override;
    void poll(uint32_t current_time_ms) noexcept override;
    void set_message_callback(MessageRecvCallback cb, void* ctx) noexcept override;
    void set_state_request_handler(StateRequestHandler handler, void* ctx) noexcept override;
    void set_command_handler(CommandHandler handler, void* ctx) noexcept override;
    bool is_ready() const noexcept override;

private:
    ITransportAdapter* adapter_;
    Config config_;
    MessageRecvCallback msg_callback_;
    void* msg_callback_ctx_;
    StateRequestHandler state_handler_;
    void* state_handler_ctx_;
    CommandHandler cmd_handler_;
    void* cmd_handler_ctx_;
    
    bool is_initialized_;

    // Frame format for UART:
    // [SOP] [SRC_ID] [DST_ID] [TYPE] [LEN_H] [LEN_L] [PAYLOAD...] [CRC_H] [CRC_L] [EOP]
    // All devices are equal, no hierarchy

    static constexpr uint8_t kSOP = 0x7E;   // Start of packet
    static constexpr uint8_t kEOP = 0x00;   // End of packet

    struct UARTFrame {
        uint8_t src_id;
        uint8_t dst_id;
        uint8_t msg_type;
        uint16_t length;
        uint8_t payload[Message::kMaxPayloadSize];
    };

    void process_received_frame_() noexcept;
    void handle_incoming_message_(const Message& msg) noexcept;
};

} // namespace ipc

#endif // IPC_UART_COMMUNICATION_HPP
