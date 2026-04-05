#include "../include/packet_protocol.hpp"

namespace ipc {

// ============================================================================
// PacketHeader Implementation
// ============================================================================

bool PacketHeader::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < kHeaderSize) {
        return false;
    }
    
    buf[0] = sop;
    buf[1] = address;
    buf[2] = static_cast<byte>(type);
    // Length in big endian (network byte order)
    buf[3] = static_cast<byte>((payload_len >> 8) & 0xFF);
    buf[4] = static_cast<byte>(payload_len & 0xFF);
    buf[5] = seq_id;
    
    return true;
}

bool PacketHeader::deserialize(const byte* buf, size_t buf_size, PacketHeader& header) noexcept {
    if (!buf || buf_size < kHeaderSize + 1) {  // +1 for seq_id
        return false;
    }
    
    if (buf[0] != 0x7E) {
        return false;  // Invalid SOP
    }
    
    header.sop = buf[0];
    header.address = buf[1];
    header.type = static_cast<PacketType>(buf[2]);
    // Length from big endian
    header.payload_len = (static_cast<uint16_t>(buf[3]) << 8) | static_cast<uint16_t>(buf[4]);
    header.seq_id = buf[5];
    
    return true;
}

// ============================================================================
// Heartbeat Payload Implementation
// ============================================================================

bool HeartbeatPayload::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    buf[0] = status;
    buf[1] = static_cast<byte>((uptime_ms >> 8) & 0xFF);
    buf[2] = static_cast<byte>(uptime_ms & 0xFF);
    
    return true;
}

bool HeartbeatPayload::deserialize(const byte* buf, size_t buf_size, HeartbeatPayload& payload) noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    payload.status = buf[0];
    payload.uptime_ms = (static_cast<uint16_t>(buf[1]) << 8) | static_cast<uint16_t>(buf[2]);
    
    return true;
}

// ============================================================================
// Ack Payload Implementation
// ============================================================================

bool AckPayload::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    buf[0] = seq_id;
    return true;
}

bool AckPayload::deserialize(const byte* buf, size_t buf_size, AckPayload& payload) noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    payload.seq_id = buf[0];
    return true;
}

// ============================================================================
// Error Payload Implementation
// ============================================================================

bool ErrorPayload::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    buf[0] = static_cast<byte>(error_code);
    buf[1] = additional_info;
    
    return true;
}

bool ErrorPayload::deserialize(const byte* buf, size_t buf_size, ErrorPayload& payload) noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    payload.error_code = static_cast<ErrorCode>(buf[0]);
    payload.additional_info = buf[1];
    
    return true;
}

// ============================================================================
// State Request Payload Implementation
// ============================================================================

bool StateRequestPayload::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    buf[0] = state_id;
    return true;
}

bool StateRequestPayload::deserialize(const byte* buf, size_t buf_size, StateRequestPayload& payload) noexcept {
    if (!buf || buf_size < kSize) {
        return false;
    }
    
    payload.state_id = buf[0];
    return true;
}

// ============================================================================
// State Response Payload Implementation
// ============================================================================

bool StateResponsePayload::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < 1 + state_data_len) {
        return false;
    }
    
    buf[0] = state_id;
    if (state_data && state_data_len > 0) {
        std::memcpy(buf + 1, state_data, state_data_len);
    }
    
    return true;
}

bool StateResponsePayload::deserialize(const byte* buf, size_t buf_size, 
                                       StateResponsePayload& payload, 
                                       byte* data_buf, size_t data_buf_size) noexcept {
    if (!buf || buf_size < 1) {
        return false;
    }
    
    payload.state_id = buf[0];
    
    if (buf_size > 1) {
        payload.state_data_len = buf_size - 1;
        if (payload.state_data_len > data_buf_size) {
            return false;
        }
        
        if (payload.state_data_len > 0) {
            std::memcpy(data_buf, buf + 1, payload.state_data_len);
            payload.state_data = data_buf;
        }
    } else {
        payload.state_data_len = 0;
        payload.state_data = nullptr;
    }
    
    return true;
}

// ============================================================================
// Command Payload Implementation
// ============================================================================

bool CommandPayload::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < 1 + command_data_len) {
        return false;
    }
    
    buf[0] = command_id;
    if (command_data && command_data_len > 0) {
        std::memcpy(buf + 1, command_data, command_data_len);
    }
    
    return true;
}

bool CommandPayload::deserialize(const byte* buf, size_t buf_size, 
                                 CommandPayload& payload, 
                                 byte* data_buf, size_t data_buf_size) noexcept {
    if (!buf || buf_size < 1) {
        return false;
    }
    
    payload.command_id = buf[0];
    
    if (buf_size > 1) {
        payload.command_data_len = buf_size - 1;
        if (payload.command_data_len > data_buf_size) {
            return false;
        }
        
        if (payload.command_data_len > 0) {
            std::memcpy(data_buf, buf + 1, payload.command_data_len);
            payload.command_data = data_buf;
        }
    } else {
        payload.command_data_len = 0;
        payload.command_data = nullptr;
    }
    
    return true;
}

// ============================================================================
// Command Response Payload Implementation
// ============================================================================

bool CommandResponsePayload::serialize(byte* buf, size_t buf_size) const noexcept {
    if (!buf || buf_size < 2 + response_data_len) {
        return false;
    }
    
    buf[0] = command_id;
    buf[1] = static_cast<byte>(status);
    if (response_data && response_data_len > 0) {
        std::memcpy(buf + 2, response_data, response_data_len);
    }
    
    return true;
}

bool CommandResponsePayload::deserialize(const byte* buf, size_t buf_size, 
                                         CommandResponsePayload& payload, 
                                         byte* data_buf, size_t data_buf_size) noexcept {
    if (!buf || buf_size < 2) {
        return false;
    }
    
    payload.command_id = buf[0];
    payload.status = static_cast<ErrorCode>(buf[1]);
    
    if (buf_size > 2) {
        payload.response_data_len = buf_size - 2;
        if (payload.response_data_len > data_buf_size) {
            return false;
        }
        
        if (payload.response_data_len > 0) {
            std::memcpy(data_buf, buf + 2, payload.response_data_len);
            payload.response_data = data_buf;
        }
    } else {
        payload.response_data_len = 0;
        payload.response_data = nullptr;
    }
    
    return true;
}

// ============================================================================
// Packet Factory Methods
// ============================================================================

Packet Packet::create_heartbeat(byte slave_addr, byte seq_id, byte status, uint16_t uptime) noexcept {
    Packet pkt;
    pkt.header = PacketHeader(slave_addr, PacketType::kService_Heartbeat, HeartbeatPayload::kSize, seq_id);
    
    HeartbeatPayload hb;
    hb.status = status;
    hb.uptime_ms = uptime;
    hb.serialize(pkt.payload, sizeof(pkt.payload));
    pkt.payload_size = HeartbeatPayload::kSize;
    
    return pkt;
}

Packet Packet::create_ack(byte slave_addr, byte seq_id) noexcept {
    Packet pkt;
    pkt.header = PacketHeader(slave_addr, PacketType::kService_Ack, AckPayload::kSize, seq_id);
    
    AckPayload ack;
    ack.seq_id = seq_id;
    ack.serialize(pkt.payload, sizeof(pkt.payload));
    pkt.payload_size = AckPayload::kSize;
    
    return pkt;
}

Packet Packet::create_error(byte slave_addr, byte seq_id, ErrorCode error) noexcept {
    Packet pkt;
    pkt.header = PacketHeader(slave_addr, PacketType::kService_Error, ErrorPayload::kSize, seq_id);
    
    ErrorPayload err;
    err.error_code = error;
    err.additional_info = 0;
    err.serialize(pkt.payload, sizeof(pkt.payload));
    pkt.payload_size = ErrorPayload::kSize;
    
    return pkt;
}

Packet Packet::create_state_request(byte slave_addr, byte seq_id, byte state_id) noexcept {
    Packet pkt;
    pkt.header = PacketHeader(slave_addr, PacketType::kState_Request, StateRequestPayload::kSize, seq_id);
    
    StateRequestPayload req;
    req.state_id = state_id;
    req.serialize(pkt.payload, sizeof(pkt.payload));
    pkt.payload_size = StateRequestPayload::kSize;
    
    return pkt;
}

Packet Packet::create_state_response(byte slave_addr, byte seq_id, byte state_id, 
                                     const byte* data, size_t len) noexcept {
    Packet pkt;
    size_t payload_len = 1 + len;  // state_id + data
    if (payload_len > StateResponsePayload::kMaxSize) {
        payload_len = StateResponsePayload::kMaxSize;
    }
    pkt.header = PacketHeader(slave_addr, PacketType::kState_Response, payload_len, seq_id);
    
    pkt.payload[0] = state_id;
    if (data && len > 0) {
        size_t copy_len = (1 + len > StateResponsePayload::kMaxSize) ? 
                         StateResponsePayload::kMaxSize - 1 : len;
        std::memcpy(pkt.payload + 1, data, copy_len);
        pkt.payload_size = 1 + copy_len;
    } else {
        pkt.payload_size = 1;
    }
    
    return pkt;
}

Packet Packet::create_command(byte slave_addr, byte seq_id, byte cmd_id, 
                             const byte* data, size_t len) noexcept {
    Packet pkt;
    size_t payload_len = 1 + len;  // cmd_id + data
    if (payload_len > CommandPayload::kMaxSize) {
        payload_len = CommandPayload::kMaxSize;
    }
    pkt.header = PacketHeader(slave_addr, PacketType::kCommand_Execute, payload_len, seq_id);
    
    pkt.payload[0] = cmd_id;
    if (data && len > 0) {
        size_t copy_len = (1 + len > CommandPayload::kMaxSize) ? 
                         CommandPayload::kMaxSize - 1 : len;
        std::memcpy(pkt.payload + 1, data, copy_len);
        pkt.payload_size = 1 + copy_len;
    } else {
        pkt.payload_size = 1;
    }
    
    return pkt;
}

Packet Packet::create_command_response(byte slave_addr, byte seq_id, byte cmd_id, 
                                       ErrorCode status, const byte* data, size_t len) noexcept {
    Packet pkt;
    size_t payload_len = 2 + len;  // cmd_id + status + data
    if (payload_len > CommandResponsePayload::kMaxSize) {
        payload_len = CommandResponsePayload::kMaxSize;
    }
    pkt.header = PacketHeader(slave_addr, PacketType::kCommand_Response, payload_len, seq_id);
    
    pkt.payload[0] = cmd_id;
    pkt.payload[1] = static_cast<byte>(status);
    if (data && len > 0) {
        size_t copy_len = (2 + len > CommandResponsePayload::kMaxSize) ? 
                         CommandResponsePayload::kMaxSize - 2 : len;
        std::memcpy(pkt.payload + 2, data, copy_len);
        pkt.payload_size = 2 + copy_len;
    } else {
        pkt.payload_size = 2;
    }
    
    return pkt;
}

} // namespace ipc
