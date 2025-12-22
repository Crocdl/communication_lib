#include <unity.h>
#include "../lib/ipc/include/ipc.hpp"
#include "../lib/ipc/include/crc.hpp"
#include "./mock_transport.hpp"

using Link = ipc::IPCLink<64, ipc::CRC16_CCITT>;

// Глобальные переменные для проверки приема
static bool rx_called = false;
static uint8_t rx_buf[64];
static size_t rx_len = 0;

// Callback функция
static void on_rx(const ipc::byte* data, size_t len, void* ctx) {
    (void)ctx;  // Не используем, но нужно для сигнатуры
    rx_called = true;
    rx_len = len;
    for (size_t i = 0; i < len && i < sizeof(rx_buf); ++i) {
        rx_buf[i] = data[i];
    }
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