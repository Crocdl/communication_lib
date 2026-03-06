#ifndef IPC_PROTOCOL_HPP
#define IPC_PROTOCOL_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace ipc {

using byte = uint8_t;

// ============================================================================
// User-Level Protocol (Same for all transports)
// ============================================================================

enum class MessageType : uint8_t {
    // User-defined message types
    kState_Update = 0x10,        // Device sends state
    kState_Request = 0x11,       // Device requests state from other
    kCommand_Execute = 0x20,     // Device sends command
    kCommand_Response = 0x21,    // Device responds to command
};

enum class ErrorCode : uint8_t {
    kNoError = 0x00,
    kCRCError = 0x01,
    kProtocolError = 0x02,
    kCommandNotSupported = 0x03,
    kCommandFailed = 0x04,
    kTimeout = 0x05,
    kBufferOverflow = 0x06,
    kInvalidState = 0x07,
    kUnknownError = 0xFF,
};

// ============================================================================
// User Protocol Message Structure
// ============================================================================

class Message {
public:
    static constexpr size_t kMaxPayloadSize = 256;
    
    uint8_t source_id;           // Sender device ID
    uint8_t dest_id;             // Recipient device ID (0xFF = broadcast)
    MessageType type;
    byte payload[kMaxPayloadSize];
    size_t payload_size;
    
    Message() noexcept 
        : source_id(0), dest_id(0xFF), type(MessageType::kState_Update), payload_size(0) {}
    
    // Helper factory methods
    static Message create_state_update(uint8_t src_id, uint8_t dst_id, 
                                       const byte* data, size_t len) noexcept {
        Message msg;
        msg.source_id = src_id;
        msg.dest_id = dst_id;
        msg.type = MessageType::kState_Update;
        msg.payload_size = (len > kMaxPayloadSize) ? kMaxPayloadSize : len;
        if (len > 0 && data) {
            std::memcpy(msg.payload, data, msg.payload_size);
        }
        return msg;
    }
    
    static Message create_state_request(uint8_t src_id, uint8_t dst_id,
                                        const byte* data, size_t len) noexcept {
        Message msg;
        msg.source_id = src_id;
        msg.dest_id = dst_id;
        msg.type = MessageType::kState_Request;
        msg.payload_size = (len > kMaxPayloadSize) ? kMaxPayloadSize : len;
        if (len > 0 && data) {
            std::memcpy(msg.payload, data, msg.payload_size);
        }
        return msg;
    }
    
    static Message create_command(uint8_t src_id, uint8_t dst_id,
                                  const byte* cmd_data, size_t cmd_len) noexcept {
        Message msg;
        msg.source_id = src_id;
        msg.dest_id = dst_id;
        msg.type = MessageType::kCommand_Execute;
        msg.payload_size = (cmd_len > kMaxPayloadSize) ? kMaxPayloadSize : cmd_len;
        if (cmd_len > 0 && cmd_data) {
            std::memcpy(msg.payload, cmd_data, msg.payload_size);
        }
        return msg;
    }
    
    static Message create_command_response(uint8_t src_id, uint8_t dst_id, 
                                          ErrorCode status, const byte* data, size_t len) noexcept {
        Message msg;
        msg.source_id = src_id;
        msg.dest_id = dst_id;
        msg.type = MessageType::kCommand_Response;
        msg.payload_size = (len + 1 > kMaxPayloadSize) ? kMaxPayloadSize - 1 : len + 1;
        msg.payload[0] = static_cast<byte>(status);
        if (len > 0 && data && msg.payload_size > 1) {
            size_t copy_len = msg.payload_size - 1;
            std::memcpy(msg.payload + 1, data, copy_len);
        }
        return msg;
    }
};

// ============================================================================
// User-Level Callbacks
// ============================================================================

using MessageRecvCallback = void(*)(const Message& msg, void* ctx);
using StateRequestHandler = size_t(*)(uint8_t dst_id, const byte* req_data, size_t req_len,
                                      byte* resp_data, size_t resp_max_len, void* ctx);
using CommandHandler = ErrorCode(*)(uint8_t dst_id, const byte* cmd_data, size_t cmd_len,
                                    byte* resp_data, size_t resp_max_len, size_t* resp_len, void* ctx);

} // namespace ipc

#endif // IPC_PROTOCOL_HPP
