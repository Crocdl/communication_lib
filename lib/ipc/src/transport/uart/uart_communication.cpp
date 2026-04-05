#include "../include/uart_communication.hpp"
#include "../../../include/utils/logger.hpp"

namespace ipc {

UARTCommunication::UARTCommunication(ITransportAdapter* adapter) noexcept
    : adapter_(adapter), msg_callback_(nullptr), msg_callback_ctx_(nullptr),
      state_handler_(nullptr), state_handler_ctx_(nullptr),
      cmd_handler_(nullptr), cmd_handler_ctx_(nullptr),
      is_initialized_(false) {
}

UARTCommunication::~UARTCommunication() noexcept {
}

bool UARTCommunication::init(const Config& config) noexcept {
    if (!adapter_) {
        LOG_ERROR("Transport adapter is null");
        return false;
    }

    config_ = config;

    // Setup RX callback
    adapter_->set_rx_handler([](byte b, void* ctx) {
        // UART adapter handles byte reception
    }, this);

    is_initialized_ = true;
    return true;
}

uint8_t UARTCommunication::get_device_id() const noexcept {
    return config_.device_id;
}

bool UARTCommunication::send_message(const Message& msg) noexcept {
    if (!is_initialized_) {
        return false;
    }

    // Format frame: [SOP] [SRC] [DST] [TYPE] [LEN_H] [LEN_L] [PAYLOAD...] [CRC_H] [CRC_L] [EOP]
    // All devices are equal - no hierarchy
    
    if (adapter_->send(msg.payload, msg.payload_size) == 0) {
        LOG_ERROR("UART send failed");
        return false;
    }
    return true;
}

void UARTCommunication::poll(uint32_t current_time_ms) noexcept {
    if (!is_initialized_) {
        return;
    }

    // Poll adapter for incoming data
    if (adapter_) {
        adapter_->poll();
    }

    process_received_frame_();
}

void UARTCommunication::set_message_callback(MessageRecvCallback cb, void* ctx) noexcept {
    msg_callback_ = cb;
    msg_callback_ctx_ = ctx;
}

void UARTCommunication::set_state_request_handler(StateRequestHandler handler, void* ctx) noexcept {
    state_handler_ = handler;
    state_handler_ctx_ = ctx;
}

void UARTCommunication::set_command_handler(CommandHandler handler, void* ctx) noexcept {
    cmd_handler_ = handler;
    cmd_handler_ctx_ = ctx;
}

bool UARTCommunication::is_ready() const noexcept {
    return is_initialized_;
}

void UARTCommunication::process_received_frame_() noexcept {
    // Implementation: read from adapter and decode frames
    // This would be integrated with the codec layer
}

void UARTCommunication::handle_incoming_message_(const Message& msg) noexcept {
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
