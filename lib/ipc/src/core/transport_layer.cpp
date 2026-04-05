#include "../include/transport_layer.hpp"
#include "../../include/utils/logger.hpp"

namespace ipc {

// ============================================================================
// Master Node Implementation
// ============================================================================

MasterNode::MasterNode(ITransportAdapter* adapter) noexcept
    : adapter_(adapter), next_seq_id_(0), packet_cb_(nullptr), 
      packet_ctx_(nullptr), is_initialized_(false) {
    
    // Initialize pending requests
    for (size_t i = 0; i < sizeof(pending_requests_) / sizeof(pending_requests_[0]); ++i) {
        pending_requests_[i].is_active = false;
    }
    
    // Setup RX callback
    if (adapter_) {
        adapter_->set_rx_handler([](byte b, void* ctx) {
            // Will be overridden by link layer
        }, this);
    }
}

MasterNode::~MasterNode() noexcept {
    // Cleanup if needed
}

bool MasterNode::init(const TransportConfig& config) noexcept {
    if (!adapter_ || config.node_address != 0x00) {
        LOG_ERROR("Master must have address 0x00");
        return false;
    }

    config_ = config;
    is_initialized_ = true;
    LOG_INFO("Master node initialized");
    return true;
}

bool MasterNode::request_state(uint8_t slave_addr, uint8_t state_id) noexcept {
    if (!is_initialized_ || slave_addr == 0x00 || slave_addr >= 0x7F) {
        return false;
    }

    Packet pkt = Packet::create_state_request(slave_addr, allocate_seq_id_(), state_id);
    
    PendingRequest* pending = find_pending_request_(pkt.header.seq_id);
    if (!pending) {
        LOG_ERROR("No free pending request slot");
        return false;
    }

    pending->seq_id = pkt.header.seq_id;
    pending->slave_address = slave_addr;
    pending->request_type = PacketType::kState_Request;
    pending->timestamp_ms = 0;  // Will be set by caller or via poll with timer
    pending->is_active = true;

    return send_packet_to_slave_(slave_addr, pkt);
}

bool MasterNode::send_command(uint8_t slave_addr, uint8_t cmd_id, 
                             const byte* cmd_data, size_t cmd_data_len) noexcept {
    if (!is_initialized_ || slave_addr == 0x00 || slave_addr >= 0x7F) {
        return false;
    }

    Packet pkt = Packet::create_command(slave_addr, allocate_seq_id_(), cmd_id, cmd_data, cmd_data_len);
    
    PendingRequest* pending = find_pending_request_(pkt.header.seq_id);
    if (!pending) {
        LOG_ERROR("No free pending request slot");
        return false;
    }

    pending->seq_id = pkt.header.seq_id;
    pending->slave_address = slave_addr;
    pending->request_type = PacketType::kCommand_Execute;
    pending->timestamp_ms = 0;
    pending->is_active = true;

    return send_packet_to_slave_(slave_addr, pkt);
}

bool MasterNode::abort_command(uint8_t slave_addr, uint8_t cmd_id) noexcept {
    if (!is_initialized_ || slave_addr == 0x00 || slave_addr >= 0x7F) {
        return false;
    }

    Packet pkt;
    pkt.header = PacketHeader(slave_addr, PacketType::kCommand_Abort, 1, allocate_seq_id_());
    pkt.payload[0] = cmd_id;
    pkt.payload_size = 1;

    return send_packet_to_slave_(slave_addr, pkt);
}

bool MasterNode::broadcast_heartbeat_request() noexcept {
    if (!is_initialized_) {
        return false;
    }

    // Send heartbeat request to broadcast address
    Packet pkt;
    pkt.header = PacketHeader(0x00, PacketType::kService_Heartbeat, 0, allocate_seq_id_());
    pkt.payload_size = 0;

    return send_packet_to_slave_(0x00, pkt);
}

void MasterNode::poll(uint32_t current_time_ms) noexcept {
    if (!is_initialized_) {
        return;
    }

    process_received_packet_();
    check_request_timeouts_(current_time_ms);
}

void MasterNode::process_received_packet_() noexcept {
    // This would be called when RX data is available
    // Implementation depends on link layer integration
}

bool MasterNode::send_packet_to_slave_(uint8_t slave_addr, const Packet& pkt) noexcept {
    if (!adapter_) {
        return false;
    }

    // Serialize and send packet through adapter
    // This will be integrated with the codec layer
    LOG_INFO("Master sending packet to slave");
    // LOG_INFO("Master sending packet to slave 0x%02X, type=0x%02X", 
    //          slave_addr, static_cast<byte>(pkt.header.type));
    
    // Placeholder - actual serialization handled by codec layer
    return true;
}

void MasterNode::handle_slave_response_(const Packet& response) noexcept {
    PendingRequest* pending = find_pending_request_(response.header.seq_id);
    if (pending) {
        pending->is_active = false;
    }

    if (packet_cb_) {
        packet_cb_(response, packet_ctx_);
    }
}

void MasterNode::check_request_timeouts_(uint32_t current_time_ms) noexcept {
    for (size_t i = 0; i < sizeof(pending_requests_) / sizeof(pending_requests_[0]); ++i) {
        if (pending_requests_[i].is_active) {
            uint32_t elapsed = current_time_ms - pending_requests_[i].timestamp_ms;
            if (elapsed > config_.request_timeout_ms) {
                pending_requests_[i].is_active = false;
            }
        }
    }
}

uint8_t MasterNode::allocate_seq_id_() noexcept {
    uint8_t seq_id = next_seq_id_;
    next_seq_id_ = (next_seq_id_ + 1) & 0x7F;  // Use only 7 bits
    return seq_id;
}

PendingRequest* MasterNode::find_pending_request_(uint8_t seq_id) noexcept {
    for (size_t i = 0; i < sizeof(pending_requests_) / sizeof(pending_requests_[0]); ++i) {
        if (!pending_requests_[i].is_active && pending_requests_[i].seq_id == seq_id) {
            return &pending_requests_[i];
        }
    }
    return nullptr;
}

const PendingRequest* MasterNode::get_pending_request(uint8_t seq_id) const noexcept {
    for (size_t i = 0; i < sizeof(pending_requests_) / sizeof(pending_requests_[0]); ++i) {
        if (pending_requests_[i].seq_id == seq_id) {
            return &pending_requests_[i];
        }
    }
    return nullptr;
}

bool MasterNode::is_waiting_for_response(uint8_t slave_addr) const noexcept {
    for (size_t i = 0; i < sizeof(pending_requests_) / sizeof(pending_requests_[0]); ++i) {
        if (pending_requests_[i].is_active && pending_requests_[i].slave_address == slave_addr) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Slave Node Implementation
// ============================================================================

SlaveNode::SlaveNode(ITransportAdapter* adapter) noexcept
    : adapter_(adapter), last_heartbeat_time_ms_(0), status_(0x01),
      state_provider_(nullptr), state_ctx_(nullptr),
      command_handler_(nullptr), command_ctx_(nullptr),
      packet_cb_(nullptr), packet_ctx_(nullptr), seq_counter_(0),
      is_initialized_(false) {
    
    if (adapter_) {
        adapter_->set_rx_handler([](byte b, void* ctx) {
            // Will be overridden by link layer
        }, this);
    }
}

SlaveNode::~SlaveNode() noexcept {
    // Cleanup if needed
}

bool SlaveNode::init(const TransportConfig& config) noexcept {
    if (!adapter_ || config.node_address == 0x00 || config.node_address >= 0x7F) {
        LOG_ERROR("Slave address must be 0x01-0x7E");
        return false;
    }

    if (!config.is_master) {
        LOG_ERROR("SlaveNode requires is_master=false");
        return false;
    }

    config_ = config;
    is_initialized_ = true;
    return true;
}

void SlaveNode::poll(uint32_t current_time_ms) noexcept {
    if (!is_initialized_) {
        return;
    }

    process_received_packet_();
    
    // Send periodic heartbeat
    if (current_time_ms - last_heartbeat_time_ms_ >= config_.heartbeat_interval_ms) {
        send_heartbeat_();
        last_heartbeat_time_ms_ = current_time_ms;
    }
}

void SlaveNode::process_received_packet_() noexcept {
    // This would be called when RX data is available
    // Implementation depends on link layer integration
}

void SlaveNode::handle_state_request_(const Packet& request) noexcept {
    if (request.payload_size < 1 || !state_provider_) {
        LOG_ERROR("Invalid state request or no state provider");
        return;
    }

    uint8_t state_id = request.payload[0];
    byte state_buffer[256];
    
    size_t state_len = state_provider_(state_id, state_buffer, sizeof(state_buffer), state_ctx_);
    
    if (state_len > 0) {
        Packet response = Packet::create_state_response(
            request.header.address,  // Master address
            request.header.seq_id,
            state_id,
            state_buffer,
            state_len
        );
        send_response_to_master_(response);
    }
}

void SlaveNode::handle_command_request_(const Packet& request) noexcept {
    if (request.payload_size < 1 || !command_handler_) {
        LOG_ERROR("Invalid command or no command handler");
        return;
    }

    uint8_t cmd_id = request.payload[0];
    const byte* cmd_data = (request.payload_size > 1) ? &request.payload[1] : nullptr;
    size_t cmd_data_len = (request.payload_size > 1) ? request.payload_size - 1 : 0;
    
    byte response_buffer[256];
    size_t response_len = 0;
    
    ErrorCode status = command_handler_(cmd_id, cmd_data, cmd_data_len,
                                        response_buffer, sizeof(response_buffer),
                                        &response_len, command_ctx_);
    
    Packet response = Packet::create_command_response(
        request.header.address,  // Master address
        request.header.seq_id,
        cmd_id,
        status,
        (response_len > 0) ? response_buffer : nullptr,
        response_len
    );
    send_response_to_master_(response);
}

bool SlaveNode::send_state_update(uint8_t state_id, const byte* state_data, size_t state_data_len) noexcept {
    if (!is_initialized_) {
        return false;
    }

    Packet pkt = Packet::create_state_response(
        0x00,  // To master (address 0x00)
        seq_counter_++,
        state_id,
        state_data,
        state_data_len
    );
    
    send_response_to_master_(pkt);
    return true;
}

void SlaveNode::send_response_to_master_(const Packet& response) noexcept {
    if (!adapter_) {
        return;
    }

    // Serialize and send packet through adapter
    // This will be integrated with the codec layer

    if (packet_cb_) {
        packet_cb_(response, packet_ctx_);
    }
}

void SlaveNode::send_heartbeat_() noexcept {
    if (!adapter_) {
        return;
    }

    Packet pkt = Packet::create_heartbeat(
        0x00,  // To master
        seq_counter_++,
        status_,
        static_cast<uint16_t>(0)  // Uptime, would be actual value
    );
    
    send_response_to_master_(pkt);
}

} // namespace ipc
