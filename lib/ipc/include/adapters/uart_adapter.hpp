#ifndef IPC_UART_ADAPTER_HPP
#define IPC_UART_ADAPTER_HPP

#include "../transport_adapter.hpp"
#ifndef PLATFORM_NATIVE
// Реальный код для ESP32
#include <HardwareSerial.h>
#else
#include "../../test/moks/HardwareSerial.h"
#include "../../test/moks/CriticalSectionStub.h"

#endif
namespace ipc {

class UARTAdapter : public ITransportAdapter {
public:
    // Конструктор с параметрами для гибкой настройки.
    UARTAdapter(HardwareSerial& uartPort = Serial, 
                uint8_t rxPin = 3, // GPIO3 по умолчанию для RX UART0[citation:7]
                uint8_t txPin = 1, // GPIO1 по умолчанию для TX UART0[citation:7]
                bool useRxBuffer = true); // Использовать буфер для снижения нагрузки в прерывании.
    
    // Метод для инициализации (вызывается один раз в setup()).
    void begin(uint32_t baudrate = 115200);
    
    size_t send(const byte*, size_t) noexcept override;
    void set_rx_handler(RxHandler, void*) noexcept override;
    void poll() noexcept override;
    void isr_rx_byte(byte b) noexcept;

private:
    static const int BUFFER_SIZE = 256; // Размер кольцевого буфера.
    void handleRxInterrupt(); // Внутренний обработчик прерывания.
    
    HardwareSerial* uartPort_;
    uint8_t rxPin_;
    uint8_t txPin_;
    bool useRxBuffer_;
    RxHandler handler_;
    void* ctx_;
    byte buffer_[BUFFER_SIZE];
    volatile int bufferHead_;
    volatile int bufferTail_;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED; // Для синхронизации в прерываниях.
};

} // namespace ipc

#endif // IPC_UART_ADAPTER_HPP