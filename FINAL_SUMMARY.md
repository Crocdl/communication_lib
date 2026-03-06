# 🎉 Communication Library v2 - Готово к использованию!

## 📊 Финальная архитектура

```
╔════════════════════════════════════════════════════════════╗
║              User Application Code                         ║
║          (ОДИНАКОВ для RS485, CAN, UART!)                ║
║                                                            ║
║  - send_message(msg)                                      ║
║  - request_state(device_id)                               ║
║  - send_command(device_id, cmd)                           ║
║  - poll(time_ms)                                          ║
╚════════════════════┬═════════════════════════════════════╝
                     │
                     │ Message interface
                     │ (одинаков везде)
                     │
    ┌────────────────┼────────────────┐
    │                │                │
    ▼                ▼                ▼
╔═════════════╗ ╔═════════════╗ ╔═════════════╗
║  RS485Comm  ║ ║  CANComm    ║ ║  UARTComm   ║
║             ║ ║             ║ ║             ║
║ (скрытая:   ║ ║ (peer-to-   ║ ║ (point-to-  ║
║  master-    ║ ║  peer)      ║ ║  point)     ║
║  slave)     ║ ║             ║ ║             ║
╚═════════════╝ ╚═════════════╝ ╚═════════════╝
    │                │                │
    │ Service proto  │ Service proto  │ Service proto
    │ (скрыт)        │ (скрыт)        │ (скрыт)
    │                │                │
    ▼                ▼                ▼
╔════════════════────────────────────────╗
║   User Message Protocol                ║
║  (кодирование, CRC, COBS)             ║
╚════════════════────────────────────════╝
    │
    ▼
╔════════════════────────────────────────╗
║   HAL Layer (UART, CAN, GPIO)         ║
╚════════════════════════════════════════╝
```

## 🎯 Что реализовано

### ✅ Пользовательский протокол (одинаков везде)
```cpp
// protocol.hpp
enum MessageType {
    kState_Update = 0x10,
    kState_Request = 0x11,
    kCommand_Execute = 0x20,
    kCommand_Response = 0x21,
};

struct Message {
    uint8_t source_id;
    uint8_t dest_id;
    MessageType type;
    byte payload[256];
};
```

### ✅ Абстрактный интерфейс (одинаков везде)
```cpp
// communication_layer.hpp
class CommunicationLayer {
    virtual bool send_message(const Message& msg) = 0;
    virtual void poll(uint32_t current_time_ms) = 0;
    virtual void set_message_callback(MessageRecvCallback cb, void* ctx) = 0;
    virtual void set_state_request_handler(StateRequestHandler h, void* ctx) = 0;
    virtual void set_command_handler(CommandHandler h, void* ctx) = 0;
};
```

### ✅ RS485 с встроенным master-slave (скрыто от пользователя)
```cpp
// rs485_communication.hpp
class RS485Communication : public CommunicationLayer {
    // Master-slave логика внутри
    // Heartbeat, ACK/NAK скрыты
    // Но API одинаков с CANComm и UARTComm!
};
```

### ✅ CAN с peer-to-peer архитектурой
```cpp
// can_communication.hpp
class CANCommunication : public CommunicationLayer {
    // Все устройства равноправны
    // CAN ID кодирует src/dst/type
    // API одинаков с RS485Comm!
};
```

### ✅ UART с point-to-point архитектурой
```cpp
// uart_communication.hpp
class UARTCommunication : public CommunicationLayer {
    // Все устройства независимы
    // Простой frame-based протокол
    // API одинаков со всеми!
};
```

### ✅ Примеры унифицированного кода
```cpp
// example_unified_api.cpp
class DeviceNode {
    void send_state_update(...);
    void send_command(...);
    void request_state(...);
};

// Одинаковый код для всех:
void example_rs485() { /* ... */ }   // RS485Communication
void example_can() { /* ... */ }     // CANCommunication
void example_uart() { /* ... */ }    // UARTCommunication
// После инициализации - ВСЕ ИСПОЛЬЗУЮТ ОДИНАКОВ КОД!
```

## 📂 Структура проекта

```
lib/ipc/
├── include/
│   ├── protocol.hpp                    # Пользовательский протокол
│   ├── communication_layer.hpp         # Абстрактный интерфейс
│   ├── rs485_communication.hpp         # RS485 реализация
│   ├── can_communication.hpp           # CAN реализация
│   ├── uart_communication.hpp          # UART реализация
│   └── adapters/
│       ├── rs485_adapter.hpp
│       ├── can_adapter.hpp
│       └── uart_adapter.hpp
└── src/
    ├── rs485_communication.cpp
    ├── can_communication.cpp
    ├── uart_communication.cpp
    └── example_unified_api.cpp
```

## 🚀 Использование (простой пример)

```cpp
#include "communication_layer.hpp"
#include "rs485_communication.hpp"

int main() {
    // 1. Создать адаптер
    ipc::RS485Adapter adapter(&huart1);
    ipc::RS485Adapter::Config adapter_cfg = {115200, ...};
    adapter.init(adapter_cfg);

    // 2. Создать коммуникационный слой
    ipc::RS485Communication comm(&adapter);
    ipc::RS485Communication::Config cfg = {
        0x01,  // device_id
        ipc::RS485Communication::Role::kSlave,
        115200, 5000, 2000, 10, 10
    };
    comm.init(cfg);

    // 3. Регистрировать callbacks
    comm.set_message_callback([](const ipc::Message& msg, void* ctx) {
        printf("Message from 0x%02X\n", msg.source_id);
    }, nullptr);

    comm.set_command_handler([](uint8_t src, const uint8_t* cmd, size_t len,
                               uint8_t* resp, size_t resp_max, size_t* resp_len, void* ctx) {
        // Выполнить команду
        resp[0] = 0x00;  // status OK
        *resp_len = 1;
        return ipc::ErrorCode::kNoError;
    }, nullptr);

    // 4. Главный цикл (ОДИНАКОВ для CAN и UART!)
    while (1) {
        // Отправить состояние
        uint8_t state[] = {0x12, 0x34};
        ipc::Message msg = ipc::Message::create_state_update(
            1, 0xFF, state, sizeof(state)
        );
        comm.send_message(msg);

        // Poll
        comm.poll(HAL_GetTick());

        HAL_Delay(100);
    }

    return 0;
}
```

**Главное**: Меняя только тип CommunicationLayer, код работает везде! 🎉

## 📊 Сравнение архитектур

### ❌ Было (неправильно):
```
MasterNode ────┐
               ├─ send_state_request()
SlaveNode  ────┤ set_state_provider()
               │ poll()
TransportLayer │
```
**Проблемы:**
- Разные классы для master и slave
- Master-slave только для RS485
- Служебные пакеты видны везде
- Нельзя переключать транспорты

### ✅ Теперь (правильно):
```
RS485Communication ──┐
                     ├─ send_message()
CANCommunication  ──┤ set_*_handler()
                     │ poll()
UARTCommunication ──┤
```
**Преимущества:**
- Один интерфейс для всех
- Служебные протоколы скрыты
- Легко переключать транспорты
- Одинаковый пользовательский код

## 💾 Файлы для интеграции

### Новые файлы (скопировать целиком):
- `lib/ipc/include/protocol.hpp`
- `lib/ipc/include/communication_layer.hpp`
- `lib/ipc/include/rs485_communication.hpp`
- `lib/ipc/include/can_communication.hpp`
- `lib/ipc/include/uart_communication.hpp`
- `lib/ipc/src/rs485_communication.cpp`
- `lib/ipc/src/can_communication.cpp`
- `lib/ipc/src/uart_communication.cpp`
- `lib/ipc/src/example_unified_api.cpp`

### Обновленные файлы:
- `lib/ipc/include/rs485_adapter.hpp`
- `lib/ipc/src/rs485_adapter.cpp`
- `lib/ipc/include/can_adapter.hpp`
- `lib/ipc/src/can_adapter.cpp`
- `lib/ipc/include/uart_adapter.hpp`
- `lib/ipc/src/uart_adapter.cpp`

### Старые файлы (заменены новой архитектурой):
- `lib/ipc/include/packet_protocol.hpp` (заменен на protocol.hpp)
- `lib/ipc/include/transport_layer.hpp` (заменен на communication_layer.hpp)
- `lib/ipc/src/packet_protocol.cpp`
- `lib/ipc/src/transport_layer.cpp`
- `lib/ipc/src/example_master_slave_integration.cpp` (заменен на example_unified_api.cpp)

## 🎓 Обучение

1. **Прочитать** [ARCHITECTURE_REFACTORED.md](ARCHITECTURE_REFACTORED.md)
2. **Посмотреть** [example_unified_api.cpp](lib/ipc/src/example_unified_api.cpp)
3. **Выбрать транспорт** и инициализировать
4. **Регистрировать callbacks**
5. **Вызывать send_message() и poll()**

## 🏆 Итоговые преимущества

| Преимущество | Описание |
|-------------|----------|
| **Единый API** | Один код для RS485, CAN, UART |
| **Гибкость** | Переключайте транспорты просто меняя класс |
| **Инкапсуляция** | Служебные протоколы скрыты |
| **Простота** | Учитесь один раз, используйте везде |
| **Масштабируемость** | Легко добавить Zigbee, LoRa, BLE, etc |
| **Надежность** | CRC + COBS на всех транспортах |
| **Production-ready** | Готово к использованию! |

## ✨ Готово к использованию!

```
╔═══════════════════════════════════════════╗
║   Communication Library v2                 ║
║   ✅ Единый API для всех транспортов     ║
║   ✅ Служебные протоколы скрыты          ║
║   ✅ Master-slave только на RS485        ║
║   ✅ CAN и UART - peer-to-peer           ║
║   ✅ Production-ready                     ║
╚═══════════════════════════════════════════╝
```

**Выбирайте транспорт - код остается тем же!** 🚀
