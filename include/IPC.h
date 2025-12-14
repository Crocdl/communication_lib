#pragma once
#include "IpcTypes.h"
#include "IpcFrame.h"
#include "IpcPolicies.h"

namespace ipc {

/**
 * Требования к Transport:
 *  - size_t write(const byte_t*, size_t)
 *  - size_t read(byte_t*, size_t)
 *  - time_ms_t time_ms()
 */
template<
    typename Transport,
    typename CrcPolicy,
    size_t   MaxPayload,
    typename RetryPolicy,
    typename TimeoutPolicy
>
class Ipc {
public:
    explicit Ipc(Transport& transport)
        : transport_(transport) {}

    Status send(const byte_t* data, size_t length);
    void poll();

    bool available() const { return app_length_ > 0; }

    size_t receive(byte_t* buffer, size_t max_length);

private:
    void process_rx();
    void handle_frame(const Frame<MaxPayload>& frame);

    Status send_frame(PacketType type,
                      const byte_t* payload,
                      size_t length);

private:
    Transport& transport_;

    uint8_t tx_seq_ = 0;
    uint8_t rx_seq_ = 0;

    Frame<MaxPayload> rx_frame_{};

    byte_t app_buffer_[MaxPayload]{};
    size_t app_length_ = 0;

    time_ms_t last_tx_time_ = 0;
    size_t retry_count_ = 0;
};

} // namespace ipc
