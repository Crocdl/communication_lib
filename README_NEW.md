# Communication Library для STM32

Полнофункциональная библиотека межпроцессного взаимодействия (IPC) с **единым API** для **всех транспортов** (RS485, CAN FD, UART). Служебные протоколы скрыты внутри каждого транспорта, пользовательское API одинаково для всех.

## 🎯 Ключевая идея

```
┌───────────────────────────────────────────────┐
│   Пользовательский код (ОДИНАКОВ ДЛЯ ВСЕХ)   │
│   - send_message()                            │
│   - request_state()                           │
│   - send_command()                            │
│   - poll()                                    │
└─────────────────────────┬─────────────────────┘
                          │
      ┌───────────────────┼───────────────────┐
      │                   │                   │
  ┌───▼────────────┐ ┌────▼────────┐ ┌──────▼─────┐
  │ RS485Comm      │ │ CANComm     │ │UARTComm    │
  │(master-slave   │ │(peer-to-    │ │(point-to-  │
  │ внутри)        │ │peer)        │ │point)      │
  └────────────────┘ └─────────────┘ └────────────┘
```

**Главное**: Пользовательский API идентичен, служебные протоколы скрыты!

## ✨ Особенности

### ✅ Унифицированный API
- Одинаковые методы для всех транспортов
- Одинаковые callback сигнатуры
- Одинаковые коды ошибок

### ✅ RS485
- Master-slave логика **скрыта внутри**
- Heartbeat и синхронизация автоматические
- Пользователь видит только `send_message()` и callbacks

### ✅ CAN
- Все устройства равноправны (peer-to-peer)
- CAN ID автоматически кодирует src, dst, type
- FD поддержка (64 байта)

### ✅ UART
- Все устройства независимы
- Простой frame-based протокол
- Идеален для отладки и первого тестирования

## 📦 Что скрыто (Service Protocol)

**RS485 (встроенный):**
- Heartbeat от slaves к master
- Master-to-slave request-response
- NAK/ACK для надежности

**CAN (встроенный):**
- CAN ID routing (source, dest, type)
- Арбитраж приоритета

**UART (встроенный):**
- Frame sync (SOP/EOP маркеры)
- Flow control при необходимости

## 📊 Пользовательский протокол (одинаков для всех)

```cpp
Message {
    source_id     // Кто отправляет
    dest_id       // Кому (0xFF = broadcast)
    type          // kState_Update, kCommand_Execute, etc
    payload[256]  // Данные
}

MessageType {
    kState_Update    (0x10)
    kState_Request   (0x11)
    kCommand_Execute (0x20)
    kCommand_Response (0x21)
}

ErrorCode {
    kNoError, kCRCError, kCommandNotSupported, ...
}
```

## 🚀 Пример (работает для всех транспортов!)

```cpp
// 1. Выберите транспорт
#include "rs485_communication.hpp"    // или can_communication.hpp, uart_communication.hpp

// 2. Создайте коммуникацию
ipc::RS485Communication comm(&adapter);
// или: ipc::CANCommunication comm(&can_adapter);
// или: ipc::UARTCommunication comm(&uart_adapter);

// 3. Инициализируйте
ipc::RS485Communication::Config cfg = {
    0x01,                                    // device_id
    ipc::RS485Communication::Role::kSlave,
    115200, 5000, 2000, 10, 10
};
comm.init(cfg);

// 4. Регистрируйте callbacks (ОДИНАКОВЫЕ)
comm.set_message_callback([](const ipc::Message& msg, void* ctx) {
    printf("Message from 0x%02X\n", msg.source_id);
}, nullptr);

comm.set_state_request_handler([](uint8_t src_id, const uint8_t* req, size_t req_len,
                                  uint8_t* resp, size_t resp_max, void* ctx) {
    // Return state data
    return 4;  // response size
}, nullptr);

comm.set_command_handler([](uint8_t src_id, const uint8_t* cmd, size_t cmd_len,
                           uint8_t* resp, size_t resp_max, size_t* resp_len, void* ctx) {
    // Execute command
    return ipc::ErrorCode::kNoError;
}, nullptr);

// 5. Главный цикл (ОДИНАКОВЫЙ)
while (1) {
    // Send state (работает на всех!)
    uint8_t state[] = {sensor1, sensor2};
    ipc::Message msg = ipc::Message::create_state_update(
        comm.get_device_id(), 0xFF, state, sizeof(state)
    );
    comm.send_message(msg);
    
    // Poll (одинаков)
    comm.poll(HAL_GetTick());
    
    HAL_Delay(100);
}
```

## 📋 Выбор транспорта

| Транспорт | Использование | API |
|-----------|---------------|-----|
| **RS485** | Длинные расстояния, master-slave | Одинаков |
| **CAN** | Быстро, peer-to-peer, CAN FD | Одинаков |
| **UART** | Отладка, простая сеть | Одинаков |

## 📂 Файлы

```
lib/ipc/include/
├── protocol.hpp              # Message, ErrorCode (одинаков для всех)
├── communication_layer.hpp   # Абстрактный интерфейс
├── rs485_communication.hpp   # RS485 (master-slave скрыт)
├── can_communication.hpp     # CAN (peer-to-peer)
├── uart_communication.hpp    # UART (point-to-point)
└── adapters/                 # Адаптеры для HAL

lib/ipc/src/
├── rs485_communication.cpp
├── can_communication.cpp
├── uart_communication.cpp
└── example_unified_api.cpp   # Примеры для всех!
```

## 🎓 Примеры кода

Смотри `lib/ipc/src/example_unified_api.cpp`:

**Пример 1: RS485** (master-slave, но одинаковый API)
```cpp
void example_rs485() {
    ipc::RS485Communication comm(&adapter);
    ipc::RS485Communication::Config cfg = {
        0x01, ipc::RS485Communication::Role::kSlave, ...
    };
    comm.init(cfg);
    // ... rest is identical for CAN/UART
}
```

**Пример 2: CAN** (peer-to-peer, но одинаковый API)
```cpp
void example_can() {
    ipc::CANCommunication comm(&adapter);
    ipc::CANCommunication::Config cfg = {
        0x05, ipc::CANCommunication::BaudRate::kBaud500k, ...
    };
    comm.init(cfg);
    // ... rest is IDENTICAL to RS485!
}
```

**Пример 3: UART** (point-to-point, одинаковый API)
```cpp
void example_uart() {
    ipc::UARTCommunication comm(&adapter);
    ipc::UARTCommunication::Config cfg = {0x01, 115200};
    comm.init(cfg);
    // ... rest is IDENTICAL!
}
```

## 💡 Преимущества

| Преимущество | Описание |
|-------------|---------|
| **Переносимость** | Один код для 3 транспортов |
| **Простота** | Учитесь один раз - используйте везде |
| **Гибкость** | Переключайте транспорты просто меняя один параметр |
| **Инкапсуляция** | Сложность скрыта в CommunicationLayer |
| **Расширяемость** | Легко добавить Zigbee, LoRa, etc |

## 🔧 Требования

- STM32 микроконтроллер (G4, H7, L4 и др.)
- STM32CubeMX
- ARM GCC toolchain

## 📚 Документация

- [QUICK_START.md](QUICK_START.md) - Быстрый старт
- [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Интеграция
- [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) - Что реализовано

## 🎉 Готово к использованию!

Выбирайте транспорт - код остается тем же! 🚀
