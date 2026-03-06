# 🗂️ Структура переорганизованной библиотеки

```
communication_lib/
├── lib/ipc/
│
├── include/
│   │
│   ├── adapters/                      # HAL адаптеры
│   │   ├── rs485_adapter.hpp         # RS485 UART адаптер
│   │   ├── uart_adapter.hpp          # UART адаптер
│   │   ├── can_adapter.hpp           # CAN FD адаптер
│   │   └── transport_adapter.hpp     # Базовый интерфейс
│   │
│   ├── core/                         # Основной функционал
│   │   ├── ipc.hpp                   # Основной API
│   │   ├── protocol.hpp              # Базовый протокол
│   │   ├── communication_layer.hpp   # Слой коммуникации
│   │   ├── codec.hpp                 # COBS + CRC кодер
│   │   ├── cobs.hpp                  # COBS алгоритм
│   │   ├── crc.hpp                   # CRC вычисления
│   │   ├── logger.hpp                # Логирование
│   │   └── packet.hpp                # Структуры пакетов
│   │
│   ├── transport/                    # Специфика транспортов
│   │   │
│   │   ├── rs485/                    # ⭐ Master-Slave архитектура
│   │   │   ├── rs485_communication.hpp
│   │   │   └── packet_protocol.hpp   # M-S протокол (скрыт)
│   │   │
│   │   ├── uart/                     # ⭐ P2P архитектура
│   │   │   └── uart_communication.hpp
│   │   │
│   │   └── can/                      # ⭐ P2P архитектура
│   │       └── can_communication.hpp
│   │
│   ├── utils/                        # Вспомогательное
│   │   └── logger.hpp
│   │
│   └── impl/                         # Реализация деталей
│       └── ipc_link_impl.hpp
│
└── src/
    │
    ├── core/                         # Реализация core
    │   └── cobs.cpp
    │
    ├── transport/                    # Реализация транспортов
    │   │
    │   ├── rs485/
    │   │   ├── rs485_communication.cpp
    │   │   ├── rs485_adapter.cpp
    │   │   ├── packet_protocol.cpp
    │   │   └── transport_layer.cpp
    │   │
    │   ├── uart/
    │   │   ├── uart_communication.cpp
    │   │   └── uart_adapter.cpp
    │   │
    │   └── can/
    │       ├── can_communication.cpp
    │       └── can_adapter.cpp
    │
    └── examples/
        ├── example_master_slave_integration.cpp
        └── example_unified_api.cpp
```

## 📊 Архитектура коммуникации

```
┌─────────────────────────────────────────────────┐
│         APPLICATION LAYER                       │
│  (Код пользователя - одинаковый для всех)     │
└────────────────────┬────────────────────────────┘
                     │
        ┌────────────▼────────────┐
        │   UNIFIED IPC API       │
        │  (include/core/ipc.hpp) │
        └────────────┬────────────┘
                     │
        ┌────────────┴──────────────────┐
        │                               │
   ┌────▼─────┐  ┌────────┐  ┌────────┐│
   │  RS485    │  │  UART  │  │  CAN   ││
   │ Transport │  │ Transport│Transport││
   └────┬─────┘  └────┬────┘  └────┬───┘
        │             │            │
        │  ┌──────────┴────────────┘│
        │  │                        │
   ┌────▼──▼────────────────────────▼──┐
   │    PACKET PROTOCOL LAYER           │
   │  (Сервис скрыт, данные видны)    │
   └────┬──────────────────────────────┘
        │
   ┌────▼──────────────────────────────┐
   │      CODEC LAYER                   │
   │  (COBS Encoding + CRC16)           │
   └────┬──────────────────────────────┘
        │
   ┌────▼──────────────────────────────┐
   │      HAL ADAPTERS                  │
   │  (RS485 GPIO DE/RE, CAN, UART)    │
   └────┬──────────────────────────────┘
        │
   ┌────▼──────────────────────────────┐
   │      STM32 HAL                     │
   │  (UART/CAN контроллер)            │
   └───────────────────────────────────┘
```

## 🔄 Типы соединений

### RS485 - Master-Slave
```
FILE PATH: include/transport/rs485/

Архитектура:
  Master ←──RS485──→ Slave
  (активно)         (активно)
  
Сервисный протокол (СКРЫТ):
  • Heartbeat - периодический сигнал от slave
  • Ack/Nak - подтверждения
  • Timeout контроль
  
Функциональные пакеты (видны приложению):
  • State Request/Response
  • Command Execute/Response
```

### UART - Point-to-Point
```
FILE PATH: include/transport/uart/

Архитектура:
  Device1 ←──UART──→ Device2
  (равные)        (равные)
  
Сервисный протокол: НЕТ
  • Прямая передача данных
  
Функциональные пакеты:
  • Полностью контролируются приложением
```

### CAN FD - Point-to-Point / Multi-node
```
FILE PATH: include/transport/can/

Архитектура:
  Node1 ←─┐
          ├──CAN FD Bus──┐
  Node2 ←─┤              ├─ Node3, Node4...
  NodeN ←─┘              └
  
Сервисный протокол: НЕТ
  • Маршрутизация по CAN ID
  
Функциональные пакеты:
  • До 64 байт (CAN FD)
  • Полностью контролируются приложением
```

## 💾 Совместимость

### Новый код (рекомендуется):
```cpp
#include "ipc/core/ipc.hpp"
#include "ipc/transport/rs485/rs485_communication.hpp"
// или uart_communication.hpp
// или can_communication.hpp
```

### Старый код (поддерживается):
```cpp
#include "ipc/rs485_communication.hpp"
// или uart_communication.hpp
// или can_communication.hpp
```

Оба варианта работают благодаря скопированным файлам.

## 🎯 Принцип организации

| Папка | Назначение | Переносимо | Масштабируемо |
|-------|-----------|-----------|---|
| `adapters/` | HAL слой | ✅ Да | ✅ Легко добавить новый |
| `core/` | Основной API | ✅ Да | ✅ Да |
| `transport/rs485/` | M-S протокол | ✅ Специфичен | ❌ Один вид |
| `transport/uart/` | P2P трансп. | ✅ Да | ✅ Да |
| `transport/can/` | P2P трансп. | ✅ Да | ✅ Да |
| `utils/` | Вспомогательное | ✅ Да | ✅ Да |

## 🚀 Использование

### Инициализация RS485
```cpp
#include "ipc/transport/rs485/rs485_communication.hpp"

ipc::RS485Link link;
link.init(config);
```

### Инициализация UART
```cpp
#include "ipc/transport/uart/uart_communication.hpp"

ipc::UARTLink link;
link.init(config);
```

### Инициализация CAN
```cpp
#include "ipc/transport/can/can_communication.hpp"

ipc::CANLink link;
link.init(config);
```

Все имеют **одинаковый API**!

## 📈 Размеры файлов

```
include/core/         ~20 KB (основное)
include/transport/    ~12 KB (специфика транспортов)
include/adapters/     ~10 KB (HAL слой)
include/utils/        ~2 KB (вспомогательное)

src/                  ~100 KB (реализация)
```

---

**Структура готова к использованию!** 🎉
