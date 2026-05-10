// ESP32 / Arduino example: UART with CRC16 + COBS framing.
// Call poll() regularly; user code only sends and receives Message payloads.

#include <Arduino.h>
#include <uart_communication.hpp>
#include <adapters/uart_adapter.hpp>

static ipc::UARTAdapter uart_adapter(Serial2, 16, 17);
static ipc::UARTCommunication uart(&uart_adapter);

static void on_message(const ipc::Message& msg, void*) {
    Serial.print("UART RX from ");
    Serial.print(msg.source_id);
    Serial.print(": ");
    for (size_t i = 0; i < msg.payload_size; ++i) {
        Serial.print(msg.payload[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    uart_adapter.begin(115200);

    ipc::UARTCommunication::Config cfg{};
    cfg.device_id = 0x01;
    cfg.baudrate = 115200;

    uart.init(cfg);
    uart.set_message_callback(on_message, nullptr);

    const ipc::byte hello[] = {'h', 'e', 'l', 'l', 'o'};
    ipc::Message msg = ipc::Message::create_state_update(cfg.device_id, 0x02, hello, sizeof(hello));
    uart.send_message(msg);
}

void loop() {
    uart.poll(millis());
}
