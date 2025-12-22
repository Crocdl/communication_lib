#include <unity.h>

// Объявления тестов
void test_cobs_basic();
void test_crc_basic();
void test_codec_basic();
void test_ipc_link_debug();
void test_ipc_link_basic();
void test_ipc_link_empty_message();
void test_ipc_link_large_message();

void setUp(void) {
    // Инициализация
}

void tearDown(void) {
    // Очистка
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    
    // Основные тесты
    RUN_TEST(test_cobs_basic);
    RUN_TEST(test_crc_basic);
    RUN_TEST(test_codec_basic);
    
    // Тесты IPCLink
    RUN_TEST(test_ipc_link_debug);
    // RUN_TEST(test_ipc_link_basic);
    RUN_TEST(test_ipc_link_empty_message);
    RUN_TEST(test_ipc_link_large_message);
    
    return UNITY_END();
}