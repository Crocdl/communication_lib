#ifndef IPC_COMMUNICATION_LAYER_HPP
#define IPC_COMMUNICATION_LAYER_HPP

#include <cstdint>
#include <cstddef>
#include "protocol.hpp"

namespace ipc {

// ============================================================================
// Communication Layer - Unified Interface for all transports
// ============================================================================

class CommunicationLayer {
public:
    virtual ~CommunicationLayer() = default;
    
    // Get device ID in the network
    virtual uint8_t get_device_id() const noexcept = 0;
    
    // Send user message (same API for all transports)
    // destination_id = 0xFF for broadcast
    virtual bool send_message(const Message& msg) noexcept = 0;
    
    // Receive and process messages
    virtual void poll(uint32_t current_time_ms) noexcept = 0;
    
    // Register message callback (called when message arrives)
    virtual void set_message_callback(MessageRecvCallback cb, void* ctx) noexcept = 0;
    
    // Register state request handler
    virtual void set_state_request_handler(StateRequestHandler handler, void* ctx) noexcept = 0;
    
    // Register command handler
    virtual void set_command_handler(CommandHandler handler, void* ctx) noexcept = 0;
    
    // Get communication status
    virtual bool is_ready() const noexcept = 0;
};

} // namespace ipc

#endif // IPC_COMMUNICATION_LAYER_HPP
