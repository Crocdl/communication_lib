#include "../include/can_communication.hpp"
#include "../../../include/utils/logger.hpp"

namespace ipc {

CANCommunication::CANCommunication(ITransportAdapter* adapter) noexcept
    : adapter_(adapter), msg_callback_(nullptr), msg_callback_ctx_(nullptr),
      state_handler_(nullptr), state_handler_ctx_(nullptr),
      cmd_handler_(nullptr), cmd_handler_ctx_(nullptr),
      is_initialized_(false) {
}

CANCommunication::~CANCommunication() noexcept {
}

bool CANCommunication::init(const Config& config) noexcept {
    if (!adapter_) {
        LOG_ERROR("Transport adapter is null");
        return false;
    }

    config_ = config;

    // Setup RX callback
    adapter_->set_rx_handler([](byte b, void* ctx) {
        // CAN adapter handles frame reception
    }, this);

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

    // Convert user message to CAN frame
    // All devices are equal in CAN - no master-slave hierarchy
    CANFrame frame;
    frame.can_id = CANFrame::encode_id(msg.source_id, msg.dest_id, msg.type);
    
    // First byte is message type, followed by payload
    frame.data[0] = static_cast<byte>(msg.type);
    frame.dlc = (msg.payload_size > 63) ? 64 : msg.payload_size + 1;
    
    if (msg.payload_size > 0) {
        std::memcpy(&frame.data[1], msg.payload, msg.payload_size);
    }

    // Send through adapter
    if (adapter_->send(frame.data, frame.dlc) == 0) {
        LOG_ERROR("CAN send failed");
        return false;
    }
    return true;
}

void CANCommunication::poll(uint32_t current_time_ms) noexcept {
    if (!is_initialized_) {
        return;
    }

    // Poll adapter for incoming frames
    if (adapter_) {
        adapter_->poll();
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
    // Implementation: read from adapter and decode frames
    // This would be integrated with the codec layer
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
