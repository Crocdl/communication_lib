#ifndef MOCK_HARDWARESERIAL_ADAPTER_HPP
#define MOCK_HARDWARESERIAL_ADAPTER_HPP

#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>

// Макросы, используемые в коде
#define SERIAL_8N1 0

// Заглушка для portMUX_TYPE
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL_ISR(x)
#define portEXIT_CRITICAL_ISR(x)

namespace ipc {

// Заглушка HardwareSerial
class HardwareSerial {
public:
    HardwareSerial() = default;

    // Инициализация UART (игнорируем пины)
    void begin(uint32_t baudrate, int config = SERIAL_8N1, uint8_t rxPin = 0, uint8_t txPin = 1) {
        baudrate_ = baudrate;
    }

    // Асинхронная запись
    size_t write(const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < len; ++i) {
            tx_queue_.push(data[i]);
        }
        return len;
    }

    // Эмуляция flush (тут просто очищаем очередь)
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!tx_queue_.empty()) tx_queue_.pop();
    }

    // Проверка, есть ли данные для чтения
    int available() {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(rx_queue_.size());
    }

    // Чтение одного байта
    uint8_t read() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (rx_queue_.empty()) return 0;
        uint8_t b = rx_queue_.front();
        rx_queue_.pop();
        return b;
    }

    // Установка обработчика прерывания приёма
    void onReceive(std::function<void()> callback) {
        rx_callback_ = callback;
    }

    // Метод для теста: эмулируем поступление байта
    void pushRxByte(uint8_t b) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            rx_queue_.push(b);
        }
        if (rx_callback_) rx_callback_();
    }

private:
    uint32_t baudrate_{0};
    std::queue<uint8_t> tx_queue_;
    std::queue<uint8_t> rx_queue_;
    std::mutex mutex_;
    std::function<void()> rx_callback_;
};

extern HardwareSerial Serial; // глобальный объект, как на Arduino

} // namespace ipc

#endif // MOCK_HARDWARESERIAL_ADAPTER_HPP