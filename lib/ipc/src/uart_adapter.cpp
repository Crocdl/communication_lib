#include "../include/adapters/uart_adapter.hpp"

#ifndef PLATFORM_NATIVE
#include <HardwareSerial.h>
#include <Arduino.h> 
#else
#include "../../test/moks/HardwareSerial.h"
#include "../../test/moks/CriticalSectionStub.h"
#define SERIAL_8N1 1
#endif
// Внешняя переменная для глобального доступа к экземпляру адаптера (опционально).

namespace ipc {

UARTAdapter::UARTAdapter(HardwareSerial& uartPort, uint8_t rxPin, uint8_t txPin, bool useRxBuffer)
    : uartPort_(&uartPort),
      rxPin_(rxPin),
      txPin_(txPin),
      useRxBuffer_(useRxBuffer),
      handler_(nullptr),
      ctx_(nullptr),
      buffer_(),
      bufferHead_(0),
      bufferTail_(0) {
}

void UARTAdapter::begin(uint32_t baudrate) {
    // Инициализация аппаратного UART порта с выбранными пинами.
    uartPort_->begin(baudrate, SERIAL_8N1, rxPin_, txPin_);

    // Настройка прерывания по приёму данных.
    // Используем лямбда-функцию, которая передаётся в обработчик прерывания.
    uartPort_->onReceive([this]() { this->handleRxInterrupt(); });

}

size_t UARTAdapter::send(const byte* data, size_t len) noexcept {
    if (!data || len == 0) {
        return 0;
    }
    
    // Асинхронная отправка. Функция запишет данные в TX буфер и вернёт управление.
    size_t written = uartPort_->write(data, len);
    
    // Можно дождаться окончания отправки (закомментировать, если не нужно).
    // uartPort_->flush();
    
    return written;
}

void UARTAdapter::set_rx_handler(RxHandler handler, void* ctx) noexcept {
    // Входим в критическую секцию для атомарного изменения указателей.
    portENTER_CRITICAL_ISR(&mux_);
    handler_ = handler;
    ctx_ = ctx;
    portEXIT_CRITICAL_ISR(&mux_);
}

void UARTAdapter::poll() noexcept {
    // В режиме с буфером проверяем, есть ли данные для обработки.
    if (useRxBuffer_) {
        portENTER_CRITICAL_ISR(&mux_);
        while (bufferHead_ != bufferTail_) {
            byte b = buffer_[bufferTail_];
            bufferTail_ = (bufferTail_ + 1) % BUFFER_SIZE;
            if (handler_) {
                // Временно выходим из критической секции для вызова обработчика.
                portEXIT_CRITICAL_ISR(&mux_);
                handler_(b, ctx_);
                portENTER_CRITICAL_ISR(&mux_);
            }
        }
        portEXIT_CRITICAL_ISR(&mux_);
    }
    // Если буфер не используется, метод `poll` ничего не делает,
    // так как данные обрабатываются сразу в прерывании.
}

void UARTAdapter::isr_rx_byte(byte b) noexcept {
    // Этот метод можно вызывать извне (например, из другого прерывания),
    // но в данной реализации он используется внутренне.
    if (handler_ && !useRxBuffer_) {
        // Если буфер не используется, вызываем обработчик сразу из прерывания.
        handler_(b, ctx_);
    } else if (useRxBuffer_) {
        // Используем кольцевой буфер для хранения данных.
        int nextHead = (bufferHead_ + 1) % BUFFER_SIZE;
        if (nextHead != bufferTail_) { // Проверяем, не переполнен ли буфер.
            buffer_[bufferHead_] = b;
            bufferHead_ = nextHead;
        }
        // Если буфер полон, данные теряются. Можно добавить счётчик ошибок.
    }
}

// Приватный метод, вызываемый из прерывания при поступлении данных.
void UARTAdapter::handleRxInterrupt() {
    while (uartPort_->available()) {
        byte b = uartPort_->read();
        isr_rx_byte(b);
    }
}

} // namespace ipc