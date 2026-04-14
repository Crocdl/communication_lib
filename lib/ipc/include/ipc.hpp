#ifndef IPC_IPC_HPP
#define IPC_IPC_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>
#include "./transport_adapter.hpp"
#include "./codec.hpp"
#include "./cobs.hpp"

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
    struct Config {
        bool use_cobs = true;
        bool use_crc = true;
        bool use_length_prefix = false;

        Config() noexcept = default;
        Config(bool cobs, bool crc, bool length_prefix) noexcept
            : use_cobs(cobs), use_crc(crc), use_length_prefix(length_prefix) {}

        static Config for_uart() noexcept {
            return Config{true, true, false};
        }

        static Config for_can_fd() noexcept {
            return Config{false, false, true};
        }

        static Config manual(bool cobs, bool crc, bool length_prefix = false) noexcept {
            return Config{cobs, crc, length_prefix};
        }
    };

    IPCLink(ITransportAdapter* adapter, RecvCallback cb, void* user_ctx = nullptr,
            const Config& config = Config::for_uart()) noexcept
        : adapter_(adapter), cb_(cb), user_ctx_(user_ctx), config_(config), rx_idx_(0),
          length_prefix_bytes_(0), expected_payload_len_(0)
    {
        stats_ = Stats();
        
        if (adapter_) {
            adapter_->set_rx_handler([](byte b, void* ctx) {
                static_cast<IPCLink*>(ctx)->on_raw_byte(b);
            }, this);
        }
    }

    void on_raw_byte(byte b) noexcept {
        if (config_.use_cobs) {
            on_raw_byte_cobs_crc_(b);
            return;
        }

        if (config_.use_length_prefix) {
            on_raw_byte_length_prefixed_(b);
            return;
        }

        // Raw stream mode without framing: treat each byte as a 1-byte payload.
        // Useful only when framing is guaranteed externally.
        if (cb_) {
            cb_(&b, 1, user_ctx_);
            stats_.frames_received++;
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

        byte raw_frame[RawPayloadBufSize];
        size_t raw_len = 0;
        if (!build_raw_frame_(payload, payload_len, raw_frame, sizeof(raw_frame), &raw_len)) {
            return false;
        }

        if (config_.use_cobs) {
            byte encoded[EncBufSize];
            size_t enc_len = cobs_encode(raw_frame, raw_len, encoded, sizeof(encoded));
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

        if (config_.use_length_prefix) {
            byte framed[RawFrameBufSize];
            framed[0] = static_cast<byte>((raw_len >> 8) & 0xFF);
            framed[1] = static_cast<byte>(raw_len & 0xFF);
            std::memcpy(framed + 2, raw_frame, raw_len);

            const size_t frame_len = raw_len + kLengthPrefixSize;
            size_t sent = adapter_->send(framed, frame_len);
            if (sent == frame_len) {
                stats_.frames_sent++;
                return true;
            }

            return false;
        }

        // Raw send without framing and without COBS
        size_t sent = adapter_->send(raw_frame, raw_len);
        if (sent == raw_len) {
            stats_.frames_sent++;
            return true;
        }

        return false;
    }

    const Stats& stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_ = Stats(); }

private:
    static constexpr size_t kLengthPrefixSize = 2;
    enum : size_t { 
        RawPayloadBufSize = MaxPayload + sizeof(typename CRC_t::value_type),
        EncBufSize = RawPayloadBufSize + (RawPayloadBufSize / 254) + 5
    };
    enum : size_t {
        RawFrameBufSize = RawPayloadBufSize + kLengthPrefixSize
    };

    ITransportAdapter* adapter_;
    RecvCallback cb_;
    void* user_ctx_;
    Config config_;
    Stats stats_;

    byte rx_buf_[EncBufSize];
    size_t rx_idx_;

    byte length_prefix_buf_[kLengthPrefixSize];
    size_t length_prefix_bytes_;
    size_t expected_payload_len_;

    byte decode_buf_[MaxPayload];

    bool build_raw_frame_(const byte* payload, size_t payload_len,
                          byte* out_raw, size_t out_size, size_t* out_len) noexcept {
        if (!payload || !out_raw || !out_len || payload_len > MaxPayload) {
            return false;
        }

        const size_t crc_size = sizeof(typename CRC_t::value_type);
        const size_t required = payload_len + (config_.use_crc ? crc_size : 0);
        if (required > out_size) {
            return false;
        }

        std::memcpy(out_raw, payload, payload_len);
        size_t raw_len = payload_len;

        if (config_.use_crc) {
            CRC_t crc;
            crc.reset();
            crc.update(payload, payload_len);
            auto crc_value = crc.finalize();
            std::memcpy(out_raw + payload_len, &crc_value, crc_size);
            raw_len += crc_size;
        }

        *out_len = raw_len;
        return true;
    }

    bool extract_payload_from_raw_(const byte* raw, size_t raw_len,
                                   byte* out_payload, size_t out_capacity,
                                   size_t* out_payload_len) noexcept {
        if (!raw || !out_payload || !out_payload_len) {
            return false;
        }

        size_t payload_len = raw_len;
        const size_t crc_size = sizeof(typename CRC_t::value_type);

        if (config_.use_crc) {
            if (raw_len < crc_size) {
                stats_.framing_errors++;
                return false;
            }
            payload_len = raw_len - crc_size;
            if (payload_len > out_capacity || payload_len > MaxPayload) {
                stats_.framing_errors++;
                return false;
            }

            typename CRC_t::value_type received_crc;
            std::memcpy(&received_crc, raw + payload_len, crc_size);

            CRC_t crc;
            crc.reset();
            crc.update(raw, payload_len);
            if (received_crc != crc.finalize()) {
                stats_.crc_errors++;
                return false;
            }
        } else if (payload_len > out_capacity || payload_len > MaxPayload) {
            stats_.framing_errors++;
            return false;
        }

        std::memcpy(out_payload, raw, payload_len);
        *out_payload_len = payload_len;
        return true;
    }

    void deliver_raw_frame_(const byte* raw, size_t raw_len) noexcept {
        size_t payload_len = 0;
        if (!extract_payload_from_raw_(raw, raw_len, decode_buf_, sizeof(decode_buf_), &payload_len)) {
            return;
        }

        stats_.frames_received++;
        if (cb_) {
            cb_(decode_buf_, payload_len, user_ctx_);
        }
    }

    void on_raw_byte_cobs_crc_(byte b) noexcept {
        if (rx_idx_ < EncBufSize) {
            rx_buf_[rx_idx_++] = b;

            // End-of-frame marker for COBS
            if (b == 0x00 && rx_idx_ > 1) {
                handle_complete_frame(rx_buf_, rx_idx_ - 1);
                rx_idx_ = 0;
            }
            return;
        }

        stats_.rx_overruns++;
        rx_idx_ = 0;
    }

    void on_raw_byte_length_prefixed_(byte b) noexcept {
        if (length_prefix_bytes_ < kLengthPrefixSize) {
            length_prefix_buf_[length_prefix_bytes_++] = b;

            if (length_prefix_bytes_ == kLengthPrefixSize) {
                expected_payload_len_ =
                    (static_cast<size_t>(length_prefix_buf_[0]) << 8) |
                    static_cast<size_t>(length_prefix_buf_[1]);

                if (expected_payload_len_ == 0 || expected_payload_len_ > MaxPayload) {
                    stats_.framing_errors++;
                    reset_length_prefixed_state_();
                }
            }

            return;
        }

        if (rx_idx_ < expected_payload_len_) {
            rx_buf_[rx_idx_++] = b;

            if (rx_idx_ == expected_payload_len_) {
                deliver_raw_frame_(rx_buf_, expected_payload_len_);
                reset_length_prefixed_state_();
            }
            return;
        }

        stats_.rx_overruns++;
        reset_length_prefixed_state_();
    }

    void reset_length_prefixed_state_() noexcept {
        length_prefix_bytes_ = 0;
        expected_payload_len_ = 0;
        rx_idx_ = 0;
    }

    void handle_complete_frame(const byte* data, size_t len) noexcept {
        byte raw[RawPayloadBufSize];
        size_t raw_len = cobs_decode(data, len, raw, sizeof(raw));
        if (raw_len == 0) {
            stats_.framing_errors++;
            return;
        }

        deliver_raw_frame_(raw, raw_len);
    }
};

} // namespace ipc

#endif // IPC_IPC_HPP
