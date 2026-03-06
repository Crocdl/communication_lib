#ifndef IPC_CAN_COMMUNICATION_HPP
#define IPC_CAN_COMMUNICATION_HPP

#include "communication_layer.hpp"
#include "transport_adapter.hpp"
#include <cstdint>

#ifdef STM32G4
#include "main.h"
#endif

namespace ipc {

// ============================================================================
// CAN Communication Layer (all devices independent, peer-to-peer)
// ============================================================================

class CANCommunication : public CommunicationLayer {
public:
    enum class BaudRate : uint32_t {
        kBaud500k = 500000,
        kBaud1M = 1000000,
        kBaud2M = 2000000,
        kBaud4M = 4000000,
        kBaud5M = 5000000,
    };

    struct Config {
        uint8_t device_id;              // Device ID in network (0x01-0x7E)
        BaudRate nominal_bitrate;       // Nominal arbitration bitrate
        BaudRate data_bitrate;          // CAN FD data bitrate
        bool enable_fd;                 // Enable CAN FD (64 bytes)
        bool enable_brs;                // Enable Bit Rate Switching
        uint32_t receive_timeout_ms;    // Timeout for receiving responses (optional)
    };

    explicit CANCommunication(ITransportAdapter* adapter) noexcept;
    ~CANCommunication() noexcept;

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

    // CAN frame format for user protocol
    struct CANFrame {
        uint32_t can_id;       // CAN ID encoding source + dest + type
        uint8_t dlc;           // Data length
        uint8_t data[64];      // Payload
        
        // CAN ID format: [2 bits type][7 bits source][7 bits dest][16 bits reserved]
        // This allows routing without master-slave hierarchy
        static uint32_t encode_id(uint8_t src, uint8_t dst, MessageType type) noexcept {
            return (static_cast<uint32_t>(type & 0x03) << 22) |
                   (static_cast<uint32_t>(src & 0x7F) << 15) |
                   (static_cast<uint32_t>(dst & 0x7F) << 8);
        }
        
        static void decode_id(uint32_t can_id, uint8_t& src, uint8_t& dst, uint8_t& type) noexcept {
            type = (can_id >> 22) & 0x03;
            src = (can_id >> 15) & 0x7F;
            dst = (can_id >> 8) & 0x7F;
        }
    };

    void process_received_frame_() noexcept;
    void handle_incoming_message_(const Message& msg) noexcept;
};

} // namespace ipc

#endif // IPC_CAN_COMMUNICATION_HPP
