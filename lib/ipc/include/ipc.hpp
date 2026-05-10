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
    uint32_t tx_overruns     = 0;
    uint32_t tx_errors       = 0;
    uint32_t rx_queued       = 0;
    uint32_t tx_queued       = 0;
    uint32_t fragments_sent  = 0;
    uint32_t fragments_received = 0;
    uint32_t reassembly_errors  = 0;
    uint32_t bytes_sent      = 0;
    uint32_t bytes_received  = 0;
};

template <size_t MaxPayload, typename CRC_t>
class IPCLink {
public:
    struct Config {
        bool use_cobs = true;
        bool use_crc = true;
        bool use_length_prefix = false;
        bool enable_tx_queue = false;
        bool enable_rx_queue = false;
        size_t physical_frame_limit = 0;

        Config() noexcept = default;
        Config(bool cobs, bool crc, bool length_prefix,
               size_t frame_limit = 0, bool tx_queue = false, bool rx_queue = false) noexcept
            : use_cobs(cobs), use_crc(crc), use_length_prefix(length_prefix),
              enable_tx_queue(tx_queue), enable_rx_queue(rx_queue),
              physical_frame_limit(frame_limit) {}

        static Config for_uart() noexcept {
            return Config{true, true, false};
        }

        static Config for_can_fd() noexcept {
            return Config{false, false, true, 64};
        }

        static Config manual(bool cobs, bool crc, bool length_prefix = false,
                             size_t frame_limit = 0,
                             bool tx_queue = false,
                             bool rx_queue = false) noexcept {
            return Config{cobs, crc, length_prefix, frame_limit, tx_queue, rx_queue};
        }
    };

    IPCLink(ITransportAdapter* adapter, RecvCallback cb, void* user_ctx = nullptr,
            const Config& config = Config::for_uart()) noexcept
        : adapter_(adapter), cb_(cb), user_ctx_(user_ctx), config_(config), rx_idx_(0),
          length_prefix_bytes_(0), expected_payload_len_(0),
          tx_head_(0), tx_tail_(0), tx_count_(0),
          rx_head_(0), rx_tail_(0), rx_count_(0),
          fragment_seq_(0)
    {
        stats_ = Stats();
        reset_reassembly_state_();
        
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
        deliver_payload_(&b, 1);
    }
    
    void process() noexcept {
        if (adapter_) {
            adapter_->poll();
        }
        flush_tx();
        drain_rx_queue_();
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

        if (should_fragment_(raw_len)) {
            return send_fragmented_raw_frame_(raw_frame, raw_len);
        }

        return send_raw_frame_(raw_frame, raw_len);
    }

    bool flush_tx() noexcept {
        bool all_sent = true;
        while (!tx_queue_empty_()) {
            const QueuedFrame& frame = tx_queue_[tx_tail_];
            if (!send_physical_frame_(frame.data, frame.len)) {
                all_sent = false;
                break;
            }
            tx_tail_ = (tx_tail_ + 1) % kTxQueueDepth;
            --tx_count_;
        }
        return all_sent;
    }

    bool receive(byte* out, size_t out_capacity, size_t* out_len) noexcept {
        if (!out || !out_len || rx_queue_empty_()) {
            return false;
        }

        const QueuedPayload& payload = rx_queue_[rx_tail_];
        if (payload.len > out_capacity) {
            stats_.framing_errors++;
            return false;
        }

        std::memcpy(out, payload.data, payload.len);
        *out_len = payload.len;
        rx_tail_ = (rx_tail_ + 1) % kRxQueueDepth;
        --rx_count_;
        return true;
    }

    size_t pending_tx() const noexcept { return tx_count_; }
    size_t available_rx() const noexcept { return rx_count_; }

    bool send_async(const byte* payload, size_t payload_len) noexcept {
        bool old = config_.enable_tx_queue;
        config_.enable_tx_queue = true;
        bool ok = send(payload, payload_len);
        config_.enable_tx_queue = old;
        return ok;
    }

    void queue_rx_payloads(bool enabled = true) noexcept {
        config_.enable_rx_queue = enabled;
    }

    void enable_queues(bool tx, bool rx) noexcept {
        config_.enable_tx_queue = tx;
        config_.enable_rx_queue = rx;
    }

    void set_config(const Config& config) noexcept {
        config_ = config;
        reset();
    }

    const Config& config() const noexcept { return config_; }

    void reset() noexcept {
        rx_idx_ = 0;
        reset_length_prefixed_state_();
        reset_reassembly_state_();
        tx_head_ = tx_tail_ = tx_count_ = 0;
        rx_head_ = rx_tail_ = rx_count_ = 0;
    }

    const Stats& stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_ = Stats(); }

private:
    static constexpr size_t kLengthPrefixSize = 2;
    static constexpr byte kFragmentMagic0 = 0xA5;
    static constexpr byte kFragmentMagic1 = 0x5A;
    static constexpr byte kFragmentFlagStart = 0x01;
    static constexpr byte kFragmentFlagEnd = 0x02;
    static constexpr size_t kFragmentHeaderSize = 9;
    static constexpr size_t kTxQueueDepth = 8;
    static constexpr size_t kRxQueueDepth = 8;
    enum : size_t { 
        RawPayloadBufSize = MaxPayload + sizeof(typename CRC_t::value_type),
        EncBufSize = RawPayloadBufSize + (RawPayloadBufSize / 254) + 6
    };
    enum : size_t {
        RawFrameBufSize = RawPayloadBufSize + kLengthPrefixSize,
        FrameBufSize = (EncBufSize > RawFrameBufSize ? EncBufSize : RawFrameBufSize)
    };

    struct QueuedFrame {
        byte data[FrameBufSize];
        size_t len;
    };

    struct QueuedPayload {
        byte data[MaxPayload];
        size_t len;
    };

    struct ReassemblyState {
        bool active;
        uint8_t seq;
        size_t total_len;
        size_t received_len;
        byte data[RawPayloadBufSize];
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
    QueuedFrame tx_queue_[kTxQueueDepth];
    size_t tx_head_;
    size_t tx_tail_;
    size_t tx_count_;
    QueuedPayload rx_queue_[kRxQueueDepth];
    size_t rx_head_;
    size_t rx_tail_;
    size_t rx_count_;
    ReassemblyState reassembly_;
    uint8_t fragment_seq_;

    bool tx_queue_empty_() const noexcept {
        return tx_count_ == 0;
    }

    bool rx_queue_empty_() const noexcept {
        return rx_count_ == 0;
    }

    bool enqueue_physical_frame_(const byte* data, size_t len) noexcept {
        if (!data || len == 0 || len > FrameBufSize || tx_count_ >= kTxQueueDepth) {
            stats_.tx_overruns++;
            return false;
        }

        std::memcpy(tx_queue_[tx_head_].data, data, len);
        tx_queue_[tx_head_].len = len;
        tx_head_ = (tx_head_ + 1) % kTxQueueDepth;
        ++tx_count_;
        stats_.tx_queued++;
        return true;
    }

    bool queue_payload_(const byte* data, size_t len) noexcept {
        if (!data || len > MaxPayload || rx_count_ >= kRxQueueDepth) {
            stats_.rx_overruns++;
            return false;
        }

        if (len > 0) {
            std::memcpy(rx_queue_[rx_head_].data, data, len);
        }
        rx_queue_[rx_head_].len = len;
        rx_head_ = (rx_head_ + 1) % kRxQueueDepth;
        ++rx_count_;
        stats_.rx_queued++;
        return true;
    }

    bool send_physical_frame_(const byte* data, size_t len) noexcept {
        if (!adapter_ || !data || len == 0) {
            return false;
        }

        if (config_.physical_frame_limit > 0 && len > config_.physical_frame_limit) {
            stats_.tx_errors++;
            return false;
        }

        const size_t sent = adapter_->send(data, len);
        if (sent == len) {
            stats_.frames_sent++;
            stats_.bytes_sent += static_cast<uint32_t>(len);
            return true;
        }

        stats_.tx_errors++;
        return false;
    }

    bool send_or_enqueue_physical_frame_(const byte* data, size_t len) noexcept {
        if (config_.enable_tx_queue) {
            return enqueue_physical_frame_(data, len);
        }

        return send_physical_frame_(data, len);
    }

    bool send_raw_frame_(const byte* raw, size_t raw_len) noexcept {
        if (!raw || raw_len == 0) {
            return false;
        }

        if (config_.use_cobs) {
            byte encoded[EncBufSize];
            size_t enc_len = cobs_encode(raw, raw_len, encoded, sizeof(encoded) - 1);
            if (enc_len == 0 || enc_len >= sizeof(encoded)) {
                stats_.framing_errors++;
                return false;
            }
            encoded[enc_len++] = 0x00;
            return send_or_enqueue_physical_frame_(encoded, enc_len);
        }

        if (config_.use_length_prefix) {
            if (raw_len + kLengthPrefixSize > FrameBufSize) {
                stats_.tx_errors++;
                return false;
            }

            byte framed[FrameBufSize];
            framed[0] = static_cast<byte>((raw_len >> 8) & 0xFF);
            framed[1] = static_cast<byte>(raw_len & 0xFF);
            std::memcpy(framed + kLengthPrefixSize, raw, raw_len);
            return send_or_enqueue_physical_frame_(framed, raw_len + kLengthPrefixSize);
        }

        return send_or_enqueue_physical_frame_(raw, raw_len);
    }

    size_t physical_overhead_() const noexcept {
        if (config_.use_length_prefix) {
            return kLengthPrefixSize;
        }
        if (config_.use_cobs) {
            return 2; // COBS code overhead is data-dependent; this is the minimum delimiter cost.
        }
        return 0;
    }

    bool should_fragment_(size_t raw_len) const noexcept {
        if (config_.physical_frame_limit == 0) {
            return false;
        }
        return raw_len + physical_overhead_() > config_.physical_frame_limit;
    }

    bool send_fragmented_raw_frame_(const byte* raw, size_t raw_len) noexcept {
        if (!raw || raw_len == 0 || config_.physical_frame_limit <= physical_overhead_() + kFragmentHeaderSize) {
            stats_.tx_errors++;
            return false;
        }

        const size_t chunk_max = config_.physical_frame_limit - physical_overhead_() - kFragmentHeaderSize;
        if (chunk_max == 0 || raw_len > RawPayloadBufSize) {
            stats_.tx_errors++;
            return false;
        }

        const uint8_t seq = ++fragment_seq_;
        size_t offset = 0;
        while (offset < raw_len) {
            const size_t remaining = raw_len - offset;
            const size_t chunk_len = remaining > chunk_max ? chunk_max : remaining;
            byte fragment[FrameBufSize];
            fragment[0] = kFragmentMagic0;
            fragment[1] = kFragmentMagic1;
            fragment[2] = seq;
            fragment[3] = static_cast<byte>((offset == 0 ? kFragmentFlagStart : 0) |
                                            (offset + chunk_len == raw_len ? kFragmentFlagEnd : 0));
            fragment[4] = static_cast<byte>((raw_len >> 8) & 0xFF);
            fragment[5] = static_cast<byte>(raw_len & 0xFF);
            fragment[6] = static_cast<byte>((offset >> 8) & 0xFF);
            fragment[7] = static_cast<byte>(offset & 0xFF);
            fragment[8] = static_cast<byte>(chunk_len);
            std::memcpy(fragment + kFragmentHeaderSize, raw + offset, chunk_len);

            if (!send_raw_frame_(fragment, kFragmentHeaderSize + chunk_len)) {
                return false;
            }

            ++stats_.fragments_sent;
            offset += chunk_len;
        }

        return true;
    }

    bool is_fragment_frame_(const byte* raw, size_t raw_len) const noexcept {
        return raw && raw_len >= kFragmentHeaderSize &&
               raw[0] == kFragmentMagic0 && raw[1] == kFragmentMagic1;
    }

    void reset_reassembly_state_() noexcept {
        reassembly_.active = false;
        reassembly_.seq = 0;
        reassembly_.total_len = 0;
        reassembly_.received_len = 0;
    }

    bool handle_fragment_frame_(const byte* raw, size_t raw_len) noexcept {
        if (!is_fragment_frame_(raw, raw_len)) {
            return false;
        }

        const uint8_t seq = raw[2];
        const uint8_t flags = raw[3];
        const size_t total_len = (static_cast<size_t>(raw[4]) << 8) | raw[5];
        const size_t offset = (static_cast<size_t>(raw[6]) << 8) | raw[7];
        const size_t chunk_len = raw[8];

        if (total_len == 0 || total_len > RawPayloadBufSize ||
            chunk_len == 0 || kFragmentHeaderSize + chunk_len != raw_len ||
            offset + chunk_len > total_len) {
            ++stats_.reassembly_errors;
            reset_reassembly_state_();
            return true;
        }

        if (flags & kFragmentFlagStart) {
            reset_reassembly_state_();
            reassembly_.active = true;
            reassembly_.seq = seq;
            reassembly_.total_len = total_len;
        }

        if (!reassembly_.active || reassembly_.seq != seq ||
            reassembly_.total_len != total_len || offset != reassembly_.received_len) {
            ++stats_.reassembly_errors;
            reset_reassembly_state_();
            return true;
        }

        std::memcpy(reassembly_.data + offset, raw + kFragmentHeaderSize, chunk_len);
        reassembly_.received_len += chunk_len;
        ++stats_.fragments_received;

        if (flags & kFragmentFlagEnd) {
            if (reassembly_.received_len == reassembly_.total_len) {
                byte completed[RawPayloadBufSize];
                const size_t completed_len = reassembly_.total_len;
                std::memcpy(completed, reassembly_.data, completed_len);
                reset_reassembly_state_();
                deliver_raw_frame_(completed, completed_len);
            } else {
                ++stats_.reassembly_errors;
                reset_reassembly_state_();
            }
        }

        return true;
    }

    void deliver_payload_(const byte* data, size_t len) noexcept {
        stats_.frames_received++;
        stats_.bytes_received += static_cast<uint32_t>(len);

        if (config_.enable_rx_queue || !cb_) {
            queue_payload_(data, len);
            return;
        }

        cb_(data, len, user_ctx_);
    }

    void drain_rx_queue_() noexcept {
        if (config_.enable_rx_queue || !cb_) {
            return;
        }

        while (!rx_queue_empty_()) {
            const QueuedPayload& payload = rx_queue_[rx_tail_];
            cb_(payload.data, payload.len, user_ctx_);
            rx_tail_ = (rx_tail_ + 1) % kRxQueueDepth;
            --rx_count_;
        }
    }

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
        if (handle_fragment_frame_(raw, raw_len)) {
            return;
        }

        size_t payload_len = 0;
        if (!extract_payload_from_raw_(raw, raw_len, decode_buf_, sizeof(decode_buf_), &payload_len)) {
            return;
        }

        deliver_payload_(decode_buf_, payload_len);
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

                if (expected_payload_len_ == 0 || expected_payload_len_ > RawPayloadBufSize) {
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
