#ifndef IPC_TRANSPORT_LAYER_HPP
#define IPC_TRANSPORT_LAYER_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "packet_protocol.hpp"
#include "transport_adapter.hpp"

namespace ipc {

// ============================================================================
// Transport Layer Configuration
// ============================================================================

struct TransportConfig {
    uint8_t node_address;           // Node address (0x00 for master, 0x01-0x7E for slaves)
    bool is_master;                 // Node role
    uint32_t request_timeout_ms;    // Timeout for command/state requests (ms)
    uint32_t heartbeat_interval_ms; // Heartbeat interval for slaves (ms)
    size_t max_pending_requests;    // Max pending requests buffer
};

// ============================================================================
// Pending Request Tracking (for Master)
// ============================================================================

struct PendingRequest {
    uint8_t seq_id;
    uint8_t slave_address;
    PacketType request_type;
    uint32_t timestamp_ms;
    bool is_active;
};

// ============================================================================
// Packet Processing Callbacks
// ============================================================================

using PacketRecvCallback = void(*)(const Packet& pkt, void* ctx);

// ============================================================================
// Master Node Implementation
// ============================================================================

class MasterNode {
public:
    explicit MasterNode(ITransportAdapter* adapter) noexcept;
    ~MasterNode() noexcept;

    bool init(const TransportConfig& config) noexcept;

    // Send state request to slave
    bool request_state(uint8_t slave_addr, uint8_t state_id) noexcept;

    // Send command to slave
    bool send_command(uint8_t slave_addr, uint8_t cmd_id, 
                     const byte* cmd_data, size_t cmd_data_len) noexcept;

    // Send command abort
    bool abort_command(uint8_t slave_addr, uint8_t cmd_id) noexcept;

    // Broadcast heartbeat request to all slaves
    bool broadcast_heartbeat_request() noexcept;

    // Poll for responses and handle timeouts
    void poll(uint32_t current_time_ms) noexcept;

    // Register packet callback
    void set_packet_callback(PacketRecvCallback cb, void* ctx) noexcept {
        packet_cb_ = cb;
        packet_ctx_ = ctx;
    }

    // Get pending request info
    const PendingRequest* get_pending_request(uint8_t seq_id) const noexcept;

    // Check if waiting for response from slave
    bool is_waiting_for_response(uint8_t slave_addr) const noexcept;

private:
    ITransportAdapter* adapter_;
    TransportConfig config_;
    PendingRequest pending_requests_[16];  // Support up to 16 simultaneous requests
    uint8_t next_seq_id_;
    PacketRecvCallback packet_cb_;
    void* packet_ctx_;
    
    bool is_initialized_;

    // Helper methods
    void process_received_packet_() noexcept;
    bool send_packet_to_slave_(uint8_t slave_addr, const Packet& pkt) noexcept;
    void handle_slave_response_(const Packet& response) noexcept;
    void check_request_timeouts_(uint32_t current_time_ms) noexcept;
    uint8_t allocate_seq_id_() noexcept;
    PendingRequest* find_pending_request_(uint8_t seq_id) noexcept;
};

// ============================================================================
// Slave Node Implementation
// ============================================================================

using StateProvider = size_t(*)(uint8_t state_id, byte* buffer, size_t buffer_size, void* ctx);
using CommandHandler = ErrorCode(*)(uint8_t cmd_id, const byte* cmd_data, size_t cmd_data_len,
                                     byte* response, size_t response_size, size_t* response_len, void* ctx);

class SlaveNode {
public:
    explicit SlaveNode(ITransportAdapter* adapter) noexcept;
    ~SlaveNode() noexcept;

    bool init(const TransportConfig& config) noexcept;

    // Register handlers
    void set_state_provider(StateProvider provider, void* ctx) noexcept {
        state_provider_ = provider;
        state_ctx_ = ctx;
    }

    void set_command_handler(CommandHandler handler, void* ctx) noexcept {
        command_handler_ = handler;
        command_ctx_ = ctx;
    }

    // Poll for incoming requests and send heartbeats
    void poll(uint32_t current_time_ms) noexcept;

    // Register packet callback (for debugging/monitoring)
    void set_packet_callback(PacketRecvCallback cb, void* ctx) noexcept {
        packet_cb_ = cb;
        packet_ctx_ = ctx;
    }

    // Unsolicited state update (send state without request)
    bool send_state_update(uint8_t state_id, const byte* state_data, size_t state_data_len) noexcept;

    // Get slave status for heartbeat
    void set_status(byte status) noexcept { status_ = status; }
    byte get_status() const noexcept { return status_; }

private:
    ITransportAdapter* adapter_;
    TransportConfig config_;
    uint32_t last_heartbeat_time_ms_;
    byte status_;
    StateProvider state_provider_;
    void* state_ctx_;
    CommandHandler command_handler_;
    void* command_ctx_;
    PacketRecvCallback packet_cb_;
    void* packet_ctx_;
    uint8_t seq_counter_;
    
    bool is_initialized_;

    // Helper methods
    void process_received_packet_() noexcept;
    void handle_state_request_(const Packet& request) noexcept;
    void handle_command_request_(const Packet& request) noexcept;
    void send_response_to_master_(const Packet& response) noexcept;
    void send_heartbeat_() noexcept;
};

} // namespace ipc

#endif // IPC_TRANSPORT_LAYER_HPP
