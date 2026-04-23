#include "../include/can_communication.hpp"
#include "../../../include/utils/logger.hpp"

namespace ipc {

CANCommunication::CANCommunication(ITransportAdapter* adapter) noexcept
    : adapter_(adapter), link_(nullptr), msg_callback_(nullptr), msg_callback_ctx_(nullptr),
      state_handler_(nullptr), state_handler_ctx_(nullptr),
      cmd_handler_(nullptr), cmd_handler_ctx_(nullptr),
      is_initialized_(false) {
}

CANCommunication::~CANCommunication() noexcept {
    delete link_;
}

bool CANCommunication::init(const Config& config) noexcept {
    if (!adapter_) {
        LOG_ERROR("Transport adapter is null");
        return false;
    }

    config_ = config;

    delete link_;
    link_ = nullptr;
    link_ = new Link(adapter_, &CANCommunication::on_link_payload_, this, Link::Config::for_can_fd());
    if (!link_) {
        LOG_ERROR("Failed to create IPCLink");
        return false;
    }

    is_initialized_ = true;
    LOG_INFO("CANCommunication initialized");
    // LOG_INFO("CANCommunication initialized: device_id=0x%02X, bitrate=%u kbps",
    //          config.device_id, config.nominal_bitrate / 1000);
    return true;
}

uint8_t CANCommunication::get_device_id() const noexcept {
    return config_.device_id;
}

bool CANCommunication::send_message(const Message& msg) noexcept {
    if (!is_initialized_) {
        return false;
    }

    byte raw[Message::kMaxPayloadSize + 5];
    size_t raw_len = 0;
    if (!encode_message_payload_(msg, raw, sizeof(raw), &raw_len)) {
        return false;
    }

    return link_ ? link_->send(raw, raw_len) : false;
}

void CANCommunication::poll(uint32_t current_time_ms) noexcept {
    if (!is_initialized_) {
        return;
    }

    if (link_) {
        link_->process();
    }

    process_received_frame_();
}

void CANCommunication::set_message_callback(MessageRecvCallback cb, void* ctx) noexcept {
    msg_callback_ = cb;
    msg_callback_ctx_ = ctx;
}

void CANCommunication::set_state_request_handler(StateRequestHandler handler, void* ctx) noexcept {
    state_handler_ = handler;
    state_handler_ctx_ = ctx;
}

void CANCommunication::set_command_handler(CommandHandler handler, void* ctx) noexcept {
    cmd_handler_ = handler;
    cmd_handler_ctx_ = ctx;
}

bool CANCommunication::is_ready() const noexcept {
    return is_initialized_;
}

void CANCommunication::process_received_frame_() noexcept {
    // RX is handled by IPCLink callback on_link_payload_.
}

bool CANCommunication::encode_message_payload_(const Message& msg, byte* out, size_t out_cap, size_t* out_len) noexcept {
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

void CANCommunication::handle_raw_payload_(const byte* data, size_t len) noexcept {
    if (!data || len < 5) {
        return;
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
        std::memcpy(msg.payload, data + 5, payload_len);
    }

    handle_incoming_message_(msg);
}

void CANCommunication::on_link_payload_(const byte* data, size_t len, void* ctx) noexcept {
    if (!ctx) {
        return;
    }
    static_cast<CANCommunication*>(ctx)->handle_raw_payload_(data, len);
}

void CANCommunication::handle_incoming_message_(const Message& msg) noexcept {
    // Check if message is for us or broadcast
    if (msg.dest_id != 0xFF && msg.dest_id != config_.device_id) {
        return;  // Not for us
    }

    // Call user callback
    if (msg_callback_) {
        msg_callback_(msg, msg_callback_ctx_);
    }

    // Handle requests if we have handlers
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

} // namespace ipc
