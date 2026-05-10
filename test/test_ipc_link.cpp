#include <unity.h>
#include "../lib/ipc/include/ipc.hpp"
#include "../lib/ipc/include/crc.hpp"
#include "./mock_transport.hpp"

using Link = ipc::IPCLink<64, ipc::CRC16_CCITT>;

// Глобальные переменные для проверки приема
static bool rx_called = false;
static uint8_t rx_buf[256];
static size_t rx_len = 0;

static void on_rx(const ipc::byte* data, size_t len, void* ctx) {
    (void)ctx;  // Не используем, но нужно для сигнатуры
    rx_called = true;
    rx_len = len;
    for (size_t i = 0; i < len && i < sizeof(rx_buf); ++i) {
        rx_buf[i] = data[i];
    }
    printf("callback rx_called addr: %p\n", (void*)&rx_called);
}

// Сброс глобальных переменных
static void reset_test_state() {
    rx_called = false;
    rx_len = 0;
    for (size_t i = 0; i < sizeof(rx_buf); ++i) {
        rx_buf[i] = 0;
    }
}

void test_ipc_link_basic() {
    // Сброс состояния перед тестом
    reset_test_state();
    
    test::MockTransport transport;
    // ВАЖНО: передаем nullptr как третий параметр
    Link link(&transport, on_rx, nullptr);
    
    const ipc::byte msg[] = {1, 2, 3, 4};
    
    // 1. Проверяем отправку
    bool send_result = link.send(msg, sizeof(msg));
    TEST_ASSERT_TRUE_MESSAGE(send_result, "send() should return true");
    
    // 2. Проверяем, что данные отправились
    TEST_ASSERT_FALSE_MESSAGE(transport.tx_data().empty(), 
                              "TX buffer should not be empty");
    
    // 3. Эмулируем получение отправленных данных
    transport.feed_rx(transport.tx_data());
    
    // 4. Даем немного "процессорного времени" для обработки
    // (если используется асинхронная обработка)
    link.process();
    
    // 5. Проверяем, что callback вызвался
    printf("test rx_called addr: %p\n", (void*)&rx_called);
    TEST_ASSERT_TRUE_MESSAGE(rx_called, "RX callback should be called");
    
    // 6. Проверяем длину полученных данных
    TEST_ASSERT_EQUAL_MESSAGE(sizeof(msg), rx_len, 
                              "Received length should match sent length");
    
    // 7. Проверяем содержимое
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(msg, rx_buf, sizeof(msg),
                                         "Received data should match sent data");
}

// Дополнительные тесты
void test_ipc_link_empty_message() {
    reset_test_state();
    
    test::MockTransport transport;
    Link link(&transport, on_rx, nullptr);
    
    // Пытаемся отправить пустое сообщение
    bool send_result = link.send(nullptr, 0);
    TEST_ASSERT_FALSE_MESSAGE(send_result, 
                             "send() with empty message should return false");
}

void test_ipc_link_large_message() {
    reset_test_state();
    
    test::MockTransport transport;
    Link link(&transport, on_rx, nullptr);
    
    // Создаем сообщение размером больше MaxPayload (64)
    ipc::byte large_msg[100];
    for (size_t i = 0; i < sizeof(large_msg); ++i) {
        large_msg[i] = i & 0xFF;
    }
    
    bool send_result = link.send(large_msg, sizeof(large_msg));
    TEST_ASSERT_FALSE_MESSAGE(send_result, 
                             "send() with too large message should return false");
}

void test_ipc_link_can_fd_mode_without_crc_cobs() {
    reset_test_state();

    test::MockTransport transport;
    Link::Config cfg = Link::Config::for_can_fd();
    Link link(&transport, on_rx, nullptr, cfg);

    const ipc::byte msg[] = {0xAA, 0xBB, 0xCC, 0xDD};
    bool send_result = link.send(msg, sizeof(msg));
    TEST_ASSERT_TRUE_MESSAGE(send_result, "send() in CAN FD mode should return true");

    const std::vector<ipc::byte>& tx = transport.tx_data();
    TEST_ASSERT_EQUAL_MESSAGE(sizeof(msg) + 2, tx.size(),
                              "CAN FD frame should contain 2-byte length prefix + payload");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x00, tx[0], "High byte of length should be 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(sizeof(msg), tx[1], "Low byte of length should match payload");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(msg, tx.data() + 2, sizeof(msg),
                                         "Payload in CAN FD mode should be raw without codec");

    transport.feed_rx(tx);
    link.process();

    TEST_ASSERT_TRUE_MESSAGE(rx_called, "RX callback should be called in CAN FD mode");
    TEST_ASSERT_EQUAL_MESSAGE(sizeof(msg), rx_len, "RX length should match in CAN FD mode");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(msg, rx_buf, sizeof(msg),
                                         "RX payload should match in CAN FD mode");
}

void test_ipc_link_manual_mode_cobs_without_crc() {
    reset_test_state();

    test::MockTransport transport;
    Link::Config cfg = Link::Config::manual(true, false, false);
    Link link(&transport, on_rx, nullptr, cfg);

    const ipc::byte msg[] = {0x10, 0x20, 0x00, 0x40}; // содержит 0x00 для проверки COBS
    bool send_result = link.send(msg, sizeof(msg));
    TEST_ASSERT_TRUE_MESSAGE(send_result, "send() in manual COBS mode should return true");

    const std::vector<ipc::byte>& tx = transport.tx_data();
    TEST_ASSERT_FALSE_MESSAGE(tx.empty(), "TX in manual COBS mode should not be empty");

    // loopback в этот же link
    transport.feed_rx(tx);
    link.process();

    TEST_ASSERT_TRUE_MESSAGE(rx_called, "RX callback should be called in manual COBS mode");
    TEST_ASSERT_EQUAL_MESSAGE(sizeof(msg), rx_len, "RX length should match in manual COBS mode");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(msg, rx_buf, sizeof(msg),
                                         "RX payload should match in manual COBS mode");
}

void test_ipc_link_can_fd_fragmentation_reassembles_large_payload() {
    reset_test_state();

    test::MockTransport transport;
    using LargeLink = ipc::IPCLink<128, ipc::CRC16_CCITT>;
    LargeLink::Config cfg = LargeLink::Config::for_can_fd();
    LargeLink link(&transport, on_rx, nullptr, cfg);

    ipc::byte msg[100];
    for (size_t i = 0; i < sizeof(msg); ++i) {
        msg[i] = static_cast<ipc::byte>(i + 1);
    }

    TEST_ASSERT_TRUE_MESSAGE(link.send(msg, sizeof(msg)),
                             "CAN FD mode should fragment large payloads");
    TEST_ASSERT_GREATER_THAN_MESSAGE(64, transport.tx_data().size(),
                                     "Fragmented payload should occupy several physical frames");

    const auto& tx = transport.tx_data();
    size_t pos = 0;
    while (pos < tx.size()) {
        TEST_ASSERT_TRUE_MESSAGE(pos + 2 <= tx.size(), "Every fragment should have a length prefix");
        size_t frame_len = (static_cast<size_t>(tx[pos]) << 8) | tx[pos + 1];
        TEST_ASSERT_TRUE_MESSAGE(frame_len + 2 <= 64,
                                 "Each CAN FD physical frame must fit into 64 bytes");
        pos += frame_len + 2;
    }
    TEST_ASSERT_EQUAL_MESSAGE(tx.size(), pos, "Fragment stream should end on frame boundary");

    transport.feed_rx(tx);
    link.process();

    TEST_ASSERT_TRUE_MESSAGE(rx_called, "RX callback should be called after reassembly");
    TEST_ASSERT_EQUAL_MESSAGE(sizeof(msg), rx_len, "Reassembled length should match");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(msg, rx_buf, sizeof(msg),
                                         "Reassembled payload should match original");
    TEST_ASSERT_GREATER_THAN_MESSAGE(1, link.stats().fragments_sent,
                                     "TX fragment counter should be updated");
    TEST_ASSERT_GREATER_THAN_MESSAGE(1, link.stats().fragments_received,
                                     "RX fragment counter should be updated");
}

void test_ipc_link_tx_queue_flushes_on_process() {
    reset_test_state();

    test::MockTransport transport;
    Link::Config cfg = Link::Config::manual(true, true, false, 0, true, false);
    Link link(&transport, on_rx, nullptr, cfg);

    const ipc::byte msg[] = {0x01, 0x02, 0x03};
    TEST_ASSERT_TRUE_MESSAGE(link.send(msg, sizeof(msg)), "Queued TX send should succeed");
    TEST_ASSERT_EQUAL_MESSAGE(1, link.pending_tx(), "Frame should be queued before process");
    TEST_ASSERT_TRUE_MESSAGE(transport.tx_data().empty(), "Transport should not send before flush");

    link.process();
    TEST_ASSERT_EQUAL_MESSAGE(0, link.pending_tx(), "TX queue should be flushed by process");
    TEST_ASSERT_FALSE_MESSAGE(transport.tx_data().empty(), "Transport should contain flushed frame");
}

void test_ipc_link_rx_queue_without_callback() {
    reset_test_state();

    test::MockTransport transport;
    Link::Config cfg = Link::Config::manual(true, true, false, 0, false, true);
    Link tx_link(&transport, nullptr, nullptr, cfg);

    const ipc::byte msg[] = {0x55, 0x66, 0x77};
    TEST_ASSERT_TRUE_MESSAGE(tx_link.send(msg, sizeof(msg)), "Send should succeed");
    transport.feed_rx(transport.tx_data());
    tx_link.process();

    ipc::byte out[16] = {};
    size_t out_len = 0;
    TEST_ASSERT_EQUAL_MESSAGE(1, tx_link.available_rx(), "RX payload should be queued");
    TEST_ASSERT_TRUE_MESSAGE(tx_link.receive(out, sizeof(out), &out_len),
                             "Queued RX payload should be readable");
    TEST_ASSERT_EQUAL_MESSAGE(sizeof(msg), out_len, "Queued RX length should match");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(msg, out, sizeof(msg),
                                         "Queued RX payload should match");
}
