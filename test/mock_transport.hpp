#ifndef TESTS_MOCK_TRANSPORT_HPP
#define TESTS_MOCK_TRANSPORT_HPP

#include <vector>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <iostream>
#include "../lib/ipc/include/transport_adapter.hpp"

namespace test {

class MockTransport : public ipc::ITransportAdapter {
public:
    using byte = ipc::byte;

    MockTransport() {
        std::cout << "MockTransport created\n";
    }

    // =========================
    // ITransportAdapter API
    // =========================

    size_t send(const byte* data, size_t len) noexcept override {
        std::cout << "MockTransport::send(" << len << " bytes)\n";
        if (len > 0 && data) {
            std::cout << "Data: ";
            for (size_t i = 0; i < len && i < 10; ++i) {
                printf("%02X ", data[i]);
            }
            if (len > 10) std::cout << "...";
            std::cout << std::endl;
            
            tx_buffer.insert(tx_buffer.end(), data, data + len);
        }
        return len;
    }

    void set_rx_handler(RxHandler handler, void* ctx) noexcept override {
        std::cout << "MockTransport::set_rx_handler()\n";
        rx_handler = handler;
        rx_ctx = ctx;
        rx_handler_set = (handler != nullptr);
    }

    void poll() noexcept override {
        // Ничего не делаем — в тестах всё управляется вручную
    }

    // =========================
    // Тестовый API (НЕ часть IPC)
    // =========================

    void clear_tx() {
        tx_buffer.clear();
    }

    const std::vector<byte>& tx_data() const {
        return tx_buffer;
    }

    void feed_rx(const byte* data, size_t len) {
        std::cout << "MockTransport::feed_rx(" << len << " bytes)\n";
        if (!rx_handler_set) {
            std::cout << "ERROR: RX handler not set!\n";
            return;
        }
        
        if (!rx_handler) {
            std::cout << "ERROR: rx_handler is null!\n";
            return;
        }
        
        for (size_t i = 0; i < len; ++i) {
            rx_handler(data[i], rx_ctx);
        }
    }

    void feed_rx(const std::vector<byte>& data) {
        feed_rx(data.data(), data.size());
    }

    bool is_rx_handler_set() const {
        return rx_handler_set;
    }

private:
    RxHandler rx_handler = nullptr;
    void* rx_ctx = nullptr;
    bool rx_handler_set = false;
    std::vector<byte> tx_buffer;
};

} // namespace test

#endif // TESTS_MOCK_TRANSPORT_HPP