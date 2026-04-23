#ifndef IPC_RS485_COMMUNICATION_HPP
#define IPC_RS485_COMMUNICATION_HPP

#include "communication_layer.hpp"
#include "transport_adapter.hpp"
#include "ipc.hpp"
#include "crc.hpp"
#include <cstdint>

#ifdef STM32G4
#include "main.h"
#endif

namespace ipc {

// ============================================================================
// RS485 Communication Layer with Master-Slave (service protocol hidden)
// ============================================================================

class RS485Communication : public CommunicationLayer {
public:
    enum class Role : uint8_t {
        kMaster = 0,
        kSlave = 1,
    };

    struct Config {
        uint8_t device_id;              // 0x00 = master, 0x01-0x7E = slave
        Role role;                      // Master or Slave
        uint32_t baudrate;              // 9600, 115200, etc.
        uint32_t heartbeat_interval_ms; // For slaves (5000 ms typical)
        uint32_t request_timeout_ms;    // For master (2000 ms typical)
        uint32_t de_assert_delay;       // GPIO DE assertion delay (µs)
        uint32_t de_deassert_delay;     // GPIO DE deassertion delay (µs)
    };

    explicit RS485Communication(ITransportAdapter* adapter) noexcept;
    ~RS485Communication() noexcept;

    bool init(const Config& config) noexcept;

    // CommunicationLayer interface
    uint8_t get_device_id() const noexcept override;
    bool send_message(const Message& msg) noexcept override;
    void poll(uint32_t current_time_ms) noexcept override;
    void set_message_callback(MessageRecvCallback cb, void* ctx) noexcept override;
    void set_state_request_handler(StateRequestHandler handler, void* ctx) noexcept override;
    void set_command_handler(CommandHandler handler, void* ctx) noexcept override;
    bool is_ready() const noexcept override;

    // Additional RS485-specific methods
    bool is_master() const noexcept { return config_.role == Role::kMaster; }
    bool is_slave() const noexcept { return config_.role == Role::kSlave; }

private:
    using Link = IPCLink<Message::kMaxPayloadSize + 5, CRC16_CCITT>;

    ITransportAdapter* adapter_;
    Link* link_;
    Config config_;
    MessageRecvCallback msg_callback_;
    void* msg_callback_ctx_;
    StateRequestHandler state_handler_;
    void* state_handler_ctx_;
    CommandHandler cmd_handler_;
    void* cmd_handler_ctx_;
    
    bool is_initialized_;
    uint32_t last_heartbeat_time_;

    // Master-only state
    struct PendingRequest {
        uint8_t slave_addr;
        uint8_t seq_id;
        uint32_t timestamp;
        bool is_active;
    };
    PendingRequest pending_requests_[16];
    uint8_t next_seq_id_;

    // Service protocol handling (hidden from user)
    void handle_service_packet_(const byte* data, size_t len) noexcept;
    void handle_user_message_(const byte* data, size_t len) noexcept;
    bool encode_message_payload_(const Message& msg, byte* out, size_t out_cap, size_t* out_len) noexcept;
    void handle_raw_payload_(const byte* data, size_t len) noexcept;
    static void on_link_payload_(const byte* data, size_t len, void* ctx) noexcept;
    
    // Master operations
    void master_send_state_request_(uint8_t slave_addr, const byte* data, size_t len) noexcept;
    void master_send_command_(uint8_t slave_addr, const byte* data, size_t len) noexcept;
    void master_process_responses_() noexcept;
    void master_check_timeouts_(uint32_t now) noexcept;
    
    // Slave operations
    void slave_send_heartbeat_() noexcept;
    void slave_process_requests_(uint32_t now) noexcept;
    void slave_send_response_(uint8_t master_addr, const Message& msg) noexcept;
};

} // namespace ipc

#endif // IPC_RS485_COMMUNICATION_HPP
