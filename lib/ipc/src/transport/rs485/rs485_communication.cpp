#include "../include/rs485_communication.hpp"
#include "../../../include/utils/logger.hpp"

namespace ipc {

RS485Communication::RS485Communication(ITransportAdapter* adapter) noexcept
    : adapter_(adapter), link_(nullptr), msg_callback_(nullptr), msg_callback_ctx_(nullptr),
      state_handler_(nullptr), state_handler_ctx_(nullptr),
      cmd_handler_(nullptr), cmd_handler_ctx_(nullptr),
      is_initialized_(false), last_heartbeat_time_(0), next_seq_id_(0) {
    
    for (size_t i = 0; i < sizeof(pending_requests_) / sizeof(pending_requests_[0]); ++i) {
        pending_requests_[i].is_active = false;
    }
}

RS485Communication::~RS485Communication() noexcept {
    delete link_;
}

bool RS485Communication::init(const Config& config) noexcept {
    if (!adapter_) {
        LOG_ERROR("Transport adapter is null");
        return false;
    }

    config_ = config;

    delete link_;
    link_ = nullptr;
    link_ = new Link(adapter_, &RS485Communication::on_link_payload_, this, Link::Config::for_uart());
    if (!link_) {
        LOG_ERROR("Failed to create IPCLink");
        return false;
    }

    is_initialized_ = true;
    return true;
}

uint8_t RS485Communication::get_device_id() const noexcept {
    return config_.device_id;
}

bool RS485Communication::send_message(const Message& msg) noexcept {
    if (!is_initialized_) {
        return false;
    }

    // For RS485, master-slave logic is hidden:
    // - Master sends requests/commands to slaves automatically
    // - Slaves receive and respond
    // - User just sends messages, transport layer handles the protocol

    byte raw[Message::kMaxPayloadSize + 5];
    size_t raw_len = 0;
    if (!encode_message_payload_(msg, raw, sizeof(raw), &raw_len)) {
        return false;
    }

    if (!link_ || !link_->send(raw, raw_len)) {
        return false;
    }

    if (config_.role == Role::kMaster) {
        if (msg.type == MessageType::kState_Request) {
            master_send_state_request_(msg.dest_id, msg.payload, msg.payload_size);
        } else if (msg.type == MessageType::kCommand_Execute) {
            master_send_command_(msg.dest_id, msg.payload, msg.payload_size);
        }
    } else {
        slave_send_response_(msg.dest_id, msg);
    }

    return true;
}

void RS485Communication::poll(uint32_t current_time_ms) noexcept {
    if (!is_initialized_) {
        return;
    }

    if (config_.role == Role::kMaster) {
        master_process_responses_();
        master_check_timeouts_(current_time_ms);
    } else {
        // Slave: send periodic heartbeat
        if (current_time_ms - last_heartbeat_time_ >= config_.heartbeat_interval_ms) {
            slave_send_heartbeat_();
            last_heartbeat_time_ = current_time_ms;
        }

        slave_process_requests_(current_time_ms);
    }

    if (link_) {
        link_->process();
    }
}

void RS485Communication::set_message_callback(MessageRecvCallback cb, void* ctx) noexcept {
    msg_callback_ = cb;
    msg_callback_ctx_ = ctx;
}

void RS485Communication::set_state_request_handler(StateRequestHandler handler, void* ctx) noexcept {
    state_handler_ = handler;
    state_handler_ctx_ = ctx;
}

void RS485Communication::set_command_handler(CommandHandler handler, void* ctx) noexcept {
    cmd_handler_ = handler;
    cmd_handler_ctx_ = ctx;
}

bool RS485Communication::is_ready() const noexcept {
    return is_initialized_;
}

void RS485Communication::handle_service_packet_(const byte* data, size_t len) noexcept {
    // Service packets (heartbeat, ack, nak) are handled internally
    // Not exposed to user
}

void RS485Communication::handle_user_message_(const byte* data, size_t len) noexcept {
    if (!data || len < 5) {
        return;  // Invalid
    }

    const size_t payload_len = (static_cast<size_t>(data[3]) << 8) | data[4];
    if (payload_len + 5 != len || payload_len > Message::kMaxPayloadSize) {
        return;
    }

    Message msg;
    msg.source_id = data[0];
    msg.dest_id = data[1];
    msg.type = static_cast<MessageType>(data[2]);

    msg.payload_size = payload_len;
    if (payload_len > 0) {
        std::memcpy(msg.payload, &data[5], payload_len);
    }

    // Check if this message is for us or broadcast
    if (msg.dest_id == 0xFF || msg.dest_id == config_.device_id) {
        // Call user callback
        if (msg_callback_) {
            msg_callback_(msg, msg_callback_ctx_);
        }

        // If it's a request, handle it
        if (msg.type == MessageType::kState_Request && state_handler_) {
            byte response_data[Message::kMaxPayloadSize];
            size_t response_len = state_handler_(msg.source_id, msg.payload, msg.payload_size,
                                                response_data, sizeof(response_data), state_handler_ctx_);
            
            if (response_len > 0) {
                Message response = Message::create_state_update(config_.device_id, msg.source_id,
                                                               response_data, response_len);
                send_message(response);
            }
        } else if (msg.type == MessageType::kCommand_Execute && cmd_handler_) {
            byte response_data[Message::kMaxPayloadSize];
            size_t response_len = 0;
            ErrorCode status = cmd_handler_(msg.source_id, msg.payload, msg.payload_size,
                                           response_data, sizeof(response_data), &response_len, cmd_handler_ctx_);
            
            Message response = Message::create_command_response(config_.device_id, msg.source_id,
                                                               status, response_data, response_len);
            send_message(response);
        }
    }
}

void RS485Communication::master_send_state_request_(uint8_t slave_addr, const byte* data, size_t len) noexcept {
    LOG_INFO("Master: requesting state from slave 0x%02X", slave_addr);
    // Implementation would wrap user message with master-slave service protocol
}

void RS485Communication::master_send_command_(uint8_t slave_addr, const byte* data, size_t len) noexcept {
    LOG_INFO("Master: sending command to slave 0x%02X", slave_addr);
    // Implementation would wrap user message with master-slave service protocol
}

void RS485Communication::master_process_responses_() noexcept {
    // Process responses from slaves
}

void RS485Communication::master_check_timeouts_(uint32_t now) noexcept {
    // Check for request timeouts
    for (size_t i = 0; i < sizeof(pending_requests_) / sizeof(pending_requests_[0]); ++i) {
        if (pending_requests_[i].is_active) {
            if (now - pending_requests_[i].timestamp > config_.request_timeout_ms) {
                pending_requests_[i].is_active = false;
            }
        }
    }
}

void RS485Communication::slave_send_heartbeat_() noexcept {
    // Send heartbeat to master (service protocol, hidden from user)
}

void RS485Communication::slave_process_requests_(uint32_t now) noexcept {
    // Process requests from master
}

void RS485Communication::slave_send_response_(uint8_t master_addr, const Message& msg) noexcept {
    // Send response to master (or master sends to other slaves)
    LOG_INFO("Slave: sending response to 0x%02X, type=0x%02X", master_addr, static_cast<byte>(msg.type));
}

bool RS485Communication::encode_message_payload_(const Message& msg, byte* out, size_t out_cap, size_t* out_len) noexcept {
    if (!out || !out_len || msg.payload_size > Message::kMaxPayloadSize) {
        return false;
    }

    const size_t total_len = msg.payload_size + 5;
    if (total_len > out_cap) {
        return false;
    }

    out[0] = msg.source_id;
    out[1] = msg.dest_id;
    out[2] = static_cast<byte>(msg.type);
    out[3] = static_cast<byte>((msg.payload_size >> 8) & 0xFF);
    out[4] = static_cast<byte>(msg.payload_size & 0xFF);
    if (msg.payload_size > 0) {
        std::memcpy(out + 5, msg.payload, msg.payload_size);
    }

    *out_len = total_len;
    return true;
}

void RS485Communication::handle_raw_payload_(const byte* data, size_t len) noexcept {
    handle_user_message_(data, len);
}

void RS485Communication::on_link_payload_(const byte* data, size_t len, void* ctx) noexcept {
    if (!ctx) {
        return;
    }
    static_cast<RS485Communication*>(ctx)->handle_raw_payload_(data, len);
}

} // namespace ipc
