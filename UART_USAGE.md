# Пример использования: UART

Ниже показан рекомендуемый сценарий работы с библиотекой на уровне `CommunicationLayer`, где низкоуровневую обработку (framing/codec) выполняет `IPCLink`.

## 1) Инициализация

```cpp
#include "lib/ipc/include/adapters/uart_adapter.hpp"
#include "lib/ipc/include/uart_communication.hpp"
#include "lib/ipc/include/protocol.hpp"

static void on_message(const ipc::Message& msg, void* ctx) {
    (void)ctx;
    // msg.source_id, msg.dest_id, msg.type, msg.payload, msg.payload_size
}

static size_t on_state_request(uint8_t src_id,
                               const ipc::byte* req_data, size_t req_len,
                               ipc::byte* resp_data, size_t resp_max_len,
                               void* ctx) {
    (void)src_id; (void)req_data; (void)req_len; (void)ctx;
    if (resp_max_len < 2) return 0;
    resp_data[0] = 0x12;
    resp_data[1] = 0x34;
    return 2;
}

static ipc::ErrorCode on_command(uint8_t src_id,
                                 const ipc::byte* cmd_data, size_t cmd_len,
                                 ipc::byte* resp_data, size_t resp_max_len,
                                 size_t* resp_len,
                                 void* ctx) {
    (void)src_id; (void)cmd_data; (void)cmd_len; (void)ctx;
    if (resp_max_len < 1) return ipc::ErrorCode::kBufferOverflow;
    resp_data[0] = 0x01;
    *resp_len = 1;
    return ipc::ErrorCode::kNoError;
}

void app_init() {
    static ipc::UARTAdapter uart;
    uart.begin(115200);

    static ipc::UARTCommunication comm(&uart);
    ipc::UARTCommunication::Config cfg{
        .device_id = 0x01,
        .baudrate = 115200
    };

    comm.init(cfg);
    comm.set_message_callback(on_message, nullptr);
    comm.set_state_request_handler(on_state_request, nullptr);
    comm.set_command_handler(on_command, nullptr);
}
```

## 2) Отправка сообщения

```cpp
void send_ping(ipc::UARTCommunication& comm) {
    const ipc::byte payload[] = {0x50, 0x49, 0x4E, 0x47}; // "PING"
    ipc::Message msg = ipc::Message::create_command(
        0x01,   // source_id
        0x02,   // dest_id
        payload,
        sizeof(payload)
    );

    comm.send_message(msg);
}
```

## 3) Главный цикл

```cpp
void app_loop(ipc::UARTCommunication& comm, uint32_t now_ms) {
    comm.poll(now_ms);
}
```

## Практические заметки

- `send_message()` принимает объект `Message` (уровень приложения).
- `poll()` запускает обработку входящего потока.
- Входящее сообщение проходит фильтрацию по `dest_id` (адрес устройства или broadcast `0xFF`).
- Для `kState_Request` и `kCommand_Execute` автоматически вызываются зарегистрированные handlers.
