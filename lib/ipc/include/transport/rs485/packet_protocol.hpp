#ifndef IPC_PACKET_PROTOCOL_HPP
#define IPC_PACKET_PROTOCOL_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace ipc {

using byte = uint8_t;

// ============================================================================
// Packet Protocol Definition for RS485 Master-Slave Architecture
// ============================================================================

// Packet types
enum class PacketType : uint8_t {
    // Service packets
    kService_Heartbeat = 0x01,      // Heartbeat from slave
    kService_Ack = 0x02,             // Acknowledgment
    kService_Nak = 0x03,             // Negative acknowledgment
    kService_Error = 0x04,           // Error notification
    
    // Functional packets - State
    kState_Request = 0x10,           // Master requests state from slave
    kState_Response = 0x11,          // Slave sends state response
    kState_Update = 0x12,            // Slave sends unsolicited state update
    
    // Functional packets - Commands
    kCommand_Execute = 0x20,         // Master sends command to slave
    kCommand_Response = 0x21,        // Slave sends command response
    kCommand_Abort = 0x22,           // Master aborts ongoing command
};

// Error codes
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
// Packet Structure
// ============================================================================
// 
// Frame Format (all fields are in network byte order - big endian):
// +--------+--------+--------+--------+--------+--------+--------+
// |  SOP   | ADDR   | TYPE   | LEN    | SEQID  | DATA   | ...    |
// +--------+--------+--------+--------+--------+--------+--------+
//   1 byte  1 byte   1 byte   2 bytes  1 byte   variable
//
// SOP:    0x7E (Start of Packet)
// ADDR:   0x00 = broadcast, 0x01-0x7E = slave address (0x7F reserved)
// TYPE:   Packet type (PacketType enum)
// LEN:    Length of DATA field (big endian)
// SEQID:  Sequence ID for request/response matching
// DATA:   Payload (0-255 bytes)
// CRC16:  CRC16-CCITT (appended by codec layer)
//
// ============================================================================

class PacketHeader {
public:
    static constexpr size_t kHeaderSize = 5;  // SOP + ADDR + TYPE + LEN(2) + SEQID
    
    PacketHeader() noexcept
        : sop(0x7E), address(0), type(PacketType::kService_Error), 
          payload_len(0), seq_id(0) {}
    
    explicit PacketHeader(uint8_t addr, PacketType pkt_type, uint16_t len, uint8_t seq) noexcept
        : sop(0x7E), address(addr), type(pkt_type), payload_len(len), seq_id(seq) {}

    byte sop;                      // Start of packet marker (0x7E)
    byte address;                  // Slave address or broadcast
    PacketType type;               // Packet type
    uint16_t payload_len;          // Length of payload
    byte seq_id;                   // Sequence ID

    // Serialize to buffer (big endian)
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    
    // Deserialize from buffer (big endian)
    static bool deserialize(const byte* buf, size_t buf_size, PacketHeader& header) noexcept;
};

// ============================================================================
// Service Packet Payloads
// ============================================================================

class HeartbeatPayload {
public:
    static constexpr size_t kSize = 3;  // status + uptime_ms (3 bytes)
    
    byte status;                   // Slave status flags
    uint16_t uptime_ms;            // Uptime in milliseconds
    
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    static bool deserialize(const byte* buf, size_t buf_size, HeartbeatPayload& payload) noexcept;
};

class AckPayload {
public:
    static constexpr size_t kSize = 1;
    
    byte seq_id;                   // Sequence ID being acknowledged
    
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    static bool deserialize(const byte* buf, size_t buf_size, AckPayload& payload) noexcept;
};

class ErrorPayload {
public:
    static constexpr size_t kSize = 2;
    
    ErrorCode error_code;          // Error code
    byte additional_info;          // Additional error information
    
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    static bool deserialize(const byte* buf, size_t buf_size, ErrorPayload& payload) noexcept;
};

// ============================================================================
// Functional Packet Payloads
// ============================================================================

class StateRequestPayload {
public:
    static constexpr size_t kSize = 1;
    
    byte state_id;                 // ID of state to request (0xFF = all states)
    
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    static bool deserialize(const byte* buf, size_t buf_size, StateRequestPayload& payload) noexcept;
};

class StateResponsePayload {
public:
    static constexpr size_t kMaxSize = 248;  // Max payload size minus header overhead
    
    byte state_id;                 // State ID
    byte* state_data;              // Pointer to state data
    size_t state_data_len;         // Length of state data
    
    StateResponsePayload() noexcept : state_id(0), state_data(nullptr), state_data_len(0) {}
    
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    static bool deserialize(const byte* buf, size_t buf_size, 
                           StateResponsePayload& payload, byte* data_buf, size_t data_buf_size) noexcept;
};

class CommandPayload {
public:
    static constexpr size_t kMaxSize = 248;
    
    byte command_id;               // Command ID
    byte* command_data;            // Pointer to command data
    size_t command_data_len;       // Length of command data
    
    CommandPayload() noexcept : command_id(0), command_data(nullptr), command_data_len(0) {}
    
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    static bool deserialize(const byte* buf, size_t buf_size, 
                           CommandPayload& payload, byte* data_buf, size_t data_buf_size) noexcept;
};

class CommandResponsePayload {
public:
    static constexpr size_t kMaxSize = 248;
    
    byte command_id;               // Original command ID
    ErrorCode status;              // Command execution status
    byte* response_data;           // Optional response data
    size_t response_data_len;      // Length of response data
    
    CommandResponsePayload() noexcept 
        : command_id(0), status(ErrorCode::kNoError), 
          response_data(nullptr), response_data_len(0) {}
    
    bool serialize(byte* buf, size_t buf_size) const noexcept;
    static bool deserialize(const byte* buf, size_t buf_size, 
                           CommandResponsePayload& payload, byte* data_buf, size_t data_buf_size) noexcept;
};

// ============================================================================
// Complete Packet Structure
// ============================================================================

class Packet {
public:
    static constexpr size_t kMaxPayloadSize = 256;
    static constexpr size_t kMaxFrameSize = PacketHeader::kHeaderSize + kMaxPayloadSize;
    
    PacketHeader header;
    byte payload[kMaxPayloadSize];
    size_t payload_size;
    
    Packet() noexcept : payload_size(0) {}
    
    // Helper to create packet headers
    static Packet create_heartbeat(byte slave_addr, byte seq_id, byte status, uint16_t uptime) noexcept;
    static Packet create_ack(byte slave_addr, byte seq_id) noexcept;
    static Packet create_error(byte slave_addr, byte seq_id, ErrorCode error) noexcept;
    static Packet create_state_request(byte slave_addr, byte seq_id, byte state_id) noexcept;
    static Packet create_state_response(byte slave_addr, byte seq_id, byte state_id, 
                                       const byte* data, size_t len) noexcept;
    static Packet create_command(byte slave_addr, byte seq_id, byte cmd_id, 
                                const byte* data, size_t len) noexcept;
    static Packet create_command_response(byte slave_addr, byte seq_id, byte cmd_id, 
                                         ErrorCode status, const byte* data, size_t len) noexcept;
};

} // namespace ipc

#endif // IPC_PACKET_PROTOCOL_HPP
