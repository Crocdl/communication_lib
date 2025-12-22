#include <unity.h>
#include <iostream>
#include <cstdio>
#include "../lib/ipc/include/ipc.hpp"
#include "../lib/ipc/include/crc.hpp"
#include "./mock_transport.hpp"

using Link = ipc::IPCLink<64, ipc::CRC16_CCITT>;

// Простая функция обратного вызова
static void dummy_rx(const ipc::byte* data, size_t len, void*) {
    printf("[RX] Received %zu bytes\n", len);
    for (size_t i = 0; i < len && i < 16; ++i) {
        printf("%02X ", data[i]);
    }
    if (len > 16) printf("...");
    printf("\n");
}

void test_ipc_link_debug() {
    printf("\n=== DEBUG: test_ipc_link_debug ===\n");
    
    // 1. Создаем транспорт
    test::MockTransport transport;
    printf("1. MockTransport created\n");
    
    // 2. Создаем линк
    Link link(&transport, dummy_rx, nullptr);
    printf("2. IPCLink created\n");
    
    // 3. Проверяем статистику до отправки
    auto stats = link.stats();
    printf("3. Initial stats: sent=%u, received=%u\n", 
           stats.frames_sent, stats.frames_received);
    
    // 4. Отправляем простое сообщение
    const ipc::byte test_msg[] = {0x01, 0x02, 0x03};
    printf("4. Attempting to send %zu bytes: ", sizeof(test_msg));
    for (auto b : test_msg) printf("%02X ", b);
    printf("\n");
    
    bool send_result = link.send(test_msg, sizeof(test_msg));
    printf("5. send() returned: %s\n", send_result ? "TRUE" : "FALSE");
    
    if (!send_result) {
        printf("ERROR: send() failed!\n");
        TEST_FAIL_MESSAGE("send() failed");
        return;
    }
    
    // 5. Проверяем, что данные появились в транспорте
    const auto& tx_data = transport.tx_data();
    printf("6. TX buffer size: %zu bytes\n", tx_data.size());
    
    if (tx_data.empty()) {
        printf("ERROR: TX buffer is empty!\n");
        TEST_FAIL_MESSAGE("TX buffer empty");
        return;
    }
    
    printf("7. TX data: ");
    for (size_t i = 0; i < tx_data.size() && i < 32; ++i) {
        printf("%02X ", tx_data[i]);
    }
    if (tx_data.size() > 32) printf("...");
    printf("\n");
    
    // 6. Проверяем статистику после отправки
    stats = link.stats();
    printf("8. Stats after send: sent=%u, received=%u\n", 
           stats.frames_sent, stats.frames_received);
    
    // 7. Эмулируем получение (пока без проверки)
    printf("9. Feeding RX with %zu bytes\n", tx_data.size());
    transport.feed_rx(tx_data);
    
    // 8. Проверяем статистику после приема
    stats = link.stats();
    printf("10. Stats after feed_rx: sent=%u, received=%u\n", 
           stats.frames_sent, stats.frames_received);
    
    printf("=== DEBUG: Test completed ===\n\n");
    TEST_PASS();
}