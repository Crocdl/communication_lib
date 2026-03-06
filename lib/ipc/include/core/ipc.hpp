#ifndef IPC_IPC_HPP
#define IPC_IPC_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "./transport_adapter.hpp"
#include "./codec.hpp"

namespace ipc {

using byte = uint8_t;

class ITransportAdapter;

using RecvCallback = void(*)(const byte*, size_t, void*);

struct Stats {
    uint32_t frames_received = 0;
    uint32_t frames_sent     = 0;
    uint32_t crc_errors      = 0;
    uint32_t framing_errors  = 0;
    uint32_t rx_overruns     = 0;
};

template <size_t MaxPayload, typename CRC_t>
class IPCLink {
public:
    IPCLink(ITransportAdapter* adapter, RecvCallback cb, void* user_ctx = nullptr) noexcept
        : adapter_(adapter), cb_(cb), user_ctx_(user_ctx), rx_idx_(0) 
    {
        stats_ = Stats();
        
        if (adapter_) {
            adapter_->set_rx_handler([](byte b, void* ctx) {
                static_cast<IPCLink*>(ctx)->on_raw_byte(b);
            }, this);
        }
    }

    void on_raw_byte(byte b) noexcept {
        if (rx_idx_ < EncBufSize) {
            rx_buf_[rx_idx_++] = b;
            
            // Проверяем окончание кадра (0x00 для COBS)
            if (b == 0x00 && rx_idx_ > 1) {
                handle_complete_frame(rx_buf_, rx_idx_);
                rx_idx_ = 0;
            }
        } else {
            stats_.rx_overruns++;
            rx_idx_ = 0;
        }
    }
    
    void process() noexcept {
        if (adapter_) {
            adapter_->poll();
        }
    }
    
    bool send(const byte* payload, size_t payload_len) noexcept {
        if (!adapter_ || !payload || payload_len == 0 || payload_len > MaxPayload) {
            return false;
        }
        
        byte encoded[EncBufSize];
        size_t enc_len = encode_frame<CRC_t>(payload, payload_len, encoded, sizeof(encoded));
        
        if (enc_len == 0) {
            return false;
        }
        
        size_t sent = adapter_->send(encoded, enc_len);
        if (sent == enc_len) {
            stats_.frames_sent++;
            return true;
        }
        
        return false;
    }

    const Stats& stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_ = Stats(); }

private:
    enum : size_t { 
        EncBufSize = MaxPayload + (MaxPayload / 254) + 5 + sizeof(typename CRC_t::value_type)
    };

    ITransportAdapter* adapter_;
    RecvCallback cb_;
    void* user_ctx_;
    Stats stats_;

    byte rx_buf_[EncBufSize];
    size_t rx_idx_;

    byte decode_buf_[MaxPayload];

    void handle_complete_frame(const byte* data, size_t len) noexcept {
        size_t payload_len;
        DecodeError err;
        
        bool success = decode_frame<CRC_t>(
            data, len, 
            decode_buf_, sizeof(decode_buf_),
            &payload_len, &err
        );
        
        if (success) {
            stats_.frames_received++;
            if (cb_) {
                cb_(decode_buf_, payload_len, user_ctx_);
            }
        } else {
            switch (err) {
                case DecodeError::CRCMismatch:
                    stats_.crc_errors++;
                    break;
                case DecodeError::MalformedCOBS:
                    stats_.framing_errors++;
                    break;
                default:
                    break;
            }
        }
    }
};

} // namespace ipc

#endif // IPC_IPC_HPP