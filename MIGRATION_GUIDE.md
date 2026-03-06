# 📋 Миграция: Старая архитектура → Новая архитектура

## ✅ Чек-лист внедрения

- [ ] Прочитать [FINAL_SUMMARY.md](FINAL_SUMMARY.md)
- [ ] Прочитать [ARCHITECTURE_REFACTORED.md](ARCHITECTURE_REFACTORED.md)
- [ ] Скопировать новые файлы (см. ниже)
- [ ] Обновить включения в коде
- [ ] Переписать инициализацию (очень просто)
- [ ] Протестировать на каждом транспорте
- [ ] Удалить старые файлы

## 📋 Что копировать

### Новые файлы заголовков (в `lib/ipc/include/`)
```
protocol.hpp                  # Новый!
communication_layer.hpp       # Новый!
rs485_communication.hpp       # Новый!
can_communication.hpp         # Новый!
uart_communication.hpp        # Новый!
```

### Новые файлы реализации (в `lib/ipc/src/`)
```
rs485_communication.cpp       # Новый!
can_communication.cpp         # Новый!
uart_communication.cpp        # Новый!
example_unified_api.cpp       # Новый пример!
```

### Обновленные адаптеры
```
lib/ipc/include/adapters/rs485_adapter.hpp
lib/ipc/include/adapters/can_adapter.hpp
lib/ipc/include/adapters/uart_adapter.hpp
lib/ipc/src/rs485_adapter.cpp
lib/ipc/src/can_adapter.cpp
lib/ipc/src/uart_adapter.cpp
```

## 🔄 Миграция кода

### ❌ Было (старое):
```cpp
#include "transport_layer.hpp"
#include "adapters/rs485_adapter.hpp"

ipc::RS485Adapter adapter(&huart1);
ipc::MasterNode master(&adapter);
ipc::TransportConfig cfg = {0x00, true, 2000, 0, 16};
master.init(cfg);

master.set_packet_callback([](const ipc::Packet& pkt, void* ctx) {
    // ...
}, nullptr);

// Совсем другой код для slave:
ipc::SlaveNode slave(&adapter);
ipc::TransportConfig cfg2 = {0x01, false, 0, 5000, 0};
slave.init(cfg2);
```

### ✅ Теперь (новое):
```cpp
#include "communication_layer.hpp"
#include "rs485_communication.hpp"
#include "adapters/rs485_adapter.hpp"

ipc::RS485Adapter adapter(&huart1);
ipc::RS485Adapter::Config adapter_cfg = {115200, ...};
adapter.init(adapter_cfg);

ipc::RS485Communication comm(&adapter);
ipc::RS485Communication::Config cfg = {
    0x01,  // device_id (0x00 = master, 0x01-0x7E = slave)
    ipc::RS485Communication::Role::kSlave,
    115200, 5000, 2000, 10, 10
};
comm.init(cfg);

// ОДИНАКОВ код для master и slave!
// Просто меняйте Role и device_id

comm.set_message_callback([](const ipc::Message& msg, void* ctx) {
    // ...
}, nullptr);
```

### Еще проще для CAN (просто меняем класс):
```cpp
#include "communication_layer.hpp"
#include "can_communication.hpp"

ipc::CANCommunication comm(&can_adapter);  // Другой класс!
ipc::CANCommunication::Config cfg = {
    0x05,  // device_id
    ipc::CANCommunication::BaudRate::kBaud500k,
    ipc::CANCommunication::BaudRate::kBaud2M,
    true, true  // FD и BRS
};
comm.init(cfg);

// Остальной код ИДЕНТИЧЕН!
comm.set_message_callback([](const ipc::Message& msg, void* ctx) {
    // ...
}, nullptr);
```

### И для UART (опять просто меняем класс):
```cpp
#include "communication_layer.hpp"
#include "uart_communication.hpp"

ipc::UARTCommunication comm(&uart_adapter);  // Другой класс!
ipc::UARTCommunication::Config cfg = {0x01, 115200};
comm.init(cfg);

// Остальной код ИДЕНТИЧЕН!
comm.set_message_callback([](const ipc::Message& msg, void* ctx) {
    // ...
}, nullptr);
```

## 🎯 Главные изменения в коде

### Было:
```cpp
// Different classes
MasterNode master;
SlaveNode slave;

// Different methods
master.request_state(slave_id, state_id);
slave.set_state_provider(...);

// Different callbacks
master.set_packet_callback(...);
slave.set_packet_callback(...);
```

### Теперь:
```cpp
// One class
RS485Communication comm;

// Same methods for everyone
comm.send_message(msg);
comm.set_message_callback(...);
comm.set_state_request_handler(...);
comm.set_command_handler(...);
comm.poll(...);
```

## 🔄 Перевод старых типов на новые

| Старое | Новое | Описание |
|--------|-------|---------|
| `Packet` | `Message` | Структура сообщения |
| `PacketType::kState_Request` | `MessageType::kState_Request` | Тип сообщения |
| `MasterNode` | `RS485Communication` | Для RS485 |
| `SlaveNode` | `RS485Communication` | Для RS485 |
| `request_state()` | `send_message(Message::create_state_request(...))` | Запрос |
| `set_packet_callback()` | `set_message_callback()` | Callback на сообщение |
| `set_state_provider()` | `set_state_request_handler()` | Обработчик запроса |
| `send_command()` | `send_message(Message::create_command(...))` | Отправка команды |

## 📝 Пример полной миграции

### Было (старое для Master на RS485):
```cpp
#include "transport_layer.hpp"
#include "adapters/rs485_adapter.hpp"

class MasterApp {
    ipc::RS485Adapter adapter;
    ipc::MasterNode master;

    void init() {
        ipc::RS485Adapter::Config cfg = {115200, ...};
        adapter.init(cfg);

        ipc::TransportConfig tcfg = {0x00, true, 2000, 0, 16};
        master.init(tcfg);

        master.set_packet_callback([this](const ipc::Packet& pkt) {
            handle_packet(pkt);
        }, nullptr);
    }

    void run() {
        master.request_state(0x01, 0);
        master.poll(HAL_GetTick());
    }

    void handle_packet(const ipc::Packet& pkt) {
        if (pkt.header.type == ipc::PacketType::kState_Response) {
            // ...
        }
    }
};
```

### Теперь (новое - ОДИНАКОВ для всех):
```cpp
#include "communication_layer.hpp"
#include "rs485_communication.hpp"

class Device {  // Может быть Master, Slave, или CAN устройство!
    ipc::RS485Communication comm;

    void init() {
        ipc::RS485Adapter::Config adapter_cfg = {115200, ...};
        // Инициализация адаптера...

        ipc::RS485Communication::Config cfg = {
            0x00,  // 0x00 для master, 0x01-0x7E для slave
            ipc::RS485Communication::Role::kMaster,  // или kSlave
            115200, 5000, 2000, 10, 10
        };
        comm.init(cfg);

        comm.set_message_callback([this](const ipc::Message& msg) {
            handle_message(msg);
        }, nullptr);

        comm.set_command_handler([this](uint8_t src, const uint8_t* cmd, size_t len,
                                       uint8_t* resp, size_t rmax, size_t* rlen) {
            return handle_command(src, cmd, len, resp, rmax, rlen);
        }, nullptr);
    }

    void run() {
        // Отправить запрос состояния
        ipc::Message msg = ipc::Message::create_state_request(
            comm.get_device_id(), 0x01, nullptr, 0
        );
        comm.send_message(msg);

        comm.poll(HAL_GetTick());
    }

    void handle_message(const ipc::Message& msg) {
        if (msg.type == ipc::MessageType::kState_Update) {
            // ...
        }
    }

    ipc::ErrorCode handle_command(uint8_t src, const uint8_t* cmd, size_t len,
                                  uint8_t* resp, size_t rmax, size_t* rlen) {
        // ...
        return ipc::ErrorCode::kNoError;
    }
};
```

## 🧪 Тестирование после миграции

### 1. RS485 Master-Slave
```bash
# Один STM32 как Master (device_id=0x00, role=kMaster)
# Другой STM32 как Slave (device_id=0x01, role=kSlave)
# Должны взаимодействовать как раньше
# Но код теперь одинаков!
```

### 2. CAN Peer-to-Peer
```bash
# Просто замените RS485Communication на CANCommunication
# Все устройства используют один и тот же код
# Коммуникация работает между всеми
```

### 3. UART Point-to-Point
```bash
# Просто замените на UARTCommunication
# Для простого тестирования
```

## ⚠️ Важные различия

1. **RS485Communication имеет Role** (Master/Slave)
   - Master: device_id=0x00
   - Slave: device_id=0x01-0x7E

2. **CANCommunication не имеет роли**
   - Все устройства используют device_id 0x01-0x7E
   - Нет иерархии

3. **UARTCommunication не имеет роли**
   - Point-to-point связь
   - device_id может быть любой

4. **Callback сигнатуры изменились**
   - `set_packet_callback()` → `set_message_callback()`
   - `set_state_provider()` → `set_state_request_handler()`
   - `set_command_handler()` остается

## 📚 Документация после миграции

Удалить старые:
- `lib/ipc/TRANSPORT_PROTOCOL.md`
- `lib/ipc/PACKET_DEFINITIONS.md`

Добавить новые:
- [ARCHITECTURE_REFACTORED.md](ARCHITECTURE_REFACTORED.md)
- [FINAL_SUMMARY.md](FINAL_SUMMARY.md)
- Этот файл

## ✅ После миграции

- [ ] Все модули компилируются
- [ ] Callbacks вызываются правильно
- [ ] Messages отправляются/получаются
- [ ] Тесты проходят
- [ ] Документация обновлена
- [ ] Старые файлы удалены
- [ ] Пример работает на RS485
- [ ] Пример работает на CAN
- [ ] Пример работает на UART

## 🎉 Готово!

После миграции у вас будет:
- ✅ Единый API для всех транспортов
- ✅ Одинаковый пользовательский код везде
- ✅ Служебные протоколы скрыты
- ✅ Легко переключать транспорты
- ✅ Легко добавлять новые транспорты
- ✅ Production-ready код

**Поздравляем с миграцией!** 🚀
