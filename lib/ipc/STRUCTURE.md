# Структура переорганизованной библиотеки

## Иерархия папок

```
lib/ipc/
├── include/
│   ├── adapters/                 # HAL адаптеры
│   │   ├── rs485_adapter.hpp
│   │   ├── uart_adapter.hpp
│   │   ├── can_adapter.hpp
│   │   └── transport_adapter.hpp (базовый интерфейс)
│   │
│   ├── core/                     # Основной уровень
│   │   ├── ipc.hpp              # Основной интерфейс
│   │   ├── protocol.hpp         # Базовый протокол
│   │   ├── communication_layer.hpp
│   │   ├── codec.hpp            # Кодирование/декодирование
│   │   ├── cobs.hpp             # COBS алгоритм
│   │   ├── crc.hpp              # CRC проверка
│   │   ├── logger.hpp           # Логирование
│   │   └── packet.hpp           # Пакеты
│   │
│   ├── transport/               # Транспортные слои
│   │   ├── rs485/              # RS485 Master-Slave
│   │   │   ├── rs485_communication.hpp
│   │   │   └── packet_protocol.hpp
│   │   ├── uart/               # UART P2P
│   │   │   └── uart_communication.hpp
│   │   └── can/                # CAN FD P2P
│   │       └── can_communication.hpp
│   │
│   ├── utils/                  # Утилиты
│   │   └── logger.hpp
│   │
│   └── impl/                   # Реализация деталей
│       └── ipc_link_impl.hpp
│
└── src/
    ├── core/                    # Базовая реализация
    │   ├── cobs.cpp
    │   └── ...
    │
    ├── transport/              # Реализация транспорта
    │   ├── rs485/
    │   │   ├── rs485_communication.cpp
    │   │   ├── rs485_adapter.cpp
    │   │   ├── packet_protocol.cpp
    │   │   └── transport_layer.cpp
    │   ├── uart/
    │   │   ├── uart_communication.cpp
    │   │   └── uart_adapter.cpp
    │   └── can/
    │       ├── can_communication.cpp
    │       └── can_adapter.cpp
    │
    ├── utils/                  # Реализация утилит
    │
    ├── example_master_slave_integration.cpp   # Пример RS485
    └── example_unified_api.cpp                # Пример унифицированного API
```

## Архитектура

```
Application Layer
    ↓
┌─ Unified API (IPC Core) ─┐
│  (одинаковый для всех)   │
└───────────┬──────────────┘
            ↓
    ┌───────────────────┬──────────────┬──────────────┐
    ↓                   ↓              ↓              ↓
┌─────────────┐  ┌────────────┐  ┌──────────┐  ┌──────────────┐
│  RS485      │  │   UART     │  │   CAN    │  │  Adapters    │
│ Master-Slave│  │    P2P     │  │   P2P    │  │ (HAL layer)  │
│  Protocol   │  │  Protocol  │  │ Protocol │  │              │
│ (hidden)    │  │  (hidden)  │  │ (hidden) │  │              │
└──────┬──────┘  └─────┬──────┘  └────┬─────┘  └──────┬───────┘
       │                │             │               │
       └────────────────┴─────────────┴───────────────┘
                        ↓
              ┌─────────────────────┐
              │   COBS + CRC16      │
              │   (Codec Layer)     │
              └─────────┬───────────┘
                        ↓
              ┌─────────────────────┐
              │   STM32 HAL         │
              │   (UART/CAN)        │
              └─────────────────────┘
```

## Типы соединений

### RS485 (Master-Slave)
- **Сервисный протокол**: Heartbeat, Ack, Error (СКРЫТО внутри)
- **Функциональные пакеты**: State Request/Response, Commands (ВИДНЫ приложению)
- **Файлы**: `include/transport/rs485/`
- **API**: Через унифицированный интерфейс

### UART (P2P - точка-точка)
- **Сервисный протокол**: ОТСУТСТВУЕТ (прямая передача данных)
- **Функциональные пакеты**: Стандартные пакеты приложения
- **Файлы**: `include/transport/uart/`
- **API**: Через унифицированный интерфейс

### CAN FD (P2P - точка-точка или multi-node)
- **Сервисный протокол**: ОТСУТСТВУЕТ (используется CAN ID для маршрутизации)
- **Функциональные пакеты**: Стандартные пакеты приложения
- **Файлы**: `include/transport/can/`
- **API**: Через унифицированный интерфейс

## Использование

### Инициализация (одинаково для всех):
```cpp
#include "ipc/ipc.hpp"
#include "ipc/transport/rs485/rs485_communication.hpp"
// или uart, или can...

ipc::RS485Link link;
link.init(config);
link.set_rx_callback([](const uint8_t* data, size_t len) {
    // Обработка данных
});

while (1) {
    // Отправить данные
    link.send(data, len);
    
    // Опросить входящие
    link.poll();
}
```

## Описание папок

| Папка | Назначение | Масштер-Slave? |
|-------|-----------|------------------|
| `adapters/` | HAL адаптеры (UART, CAN) | N/A |
| `core/` | Основной API, кодеки, утилиты | Нет |
| `transport/rs485/` | RS485 с M-S протоколом | ДА |
| `transport/uart/` | UART P2P (P2P) | НЕТ |
| `transport/can/` | CAN FD (P2P) | НЕТ |
| `utils/` | Вспомогательные функции | N/A |

## Включение файлов

### Для RS485 Master-Slave:
```cpp
#include "ipc/transport/rs485/rs485_communication.hpp"
```

### Для UART P2P:
```cpp
#include "ipc/transport/uart/uart_communication.hpp"
```

### Для CAN FD:
```cpp
#include "ipc/transport/can/can_communication.hpp"
```

### Основной API:
```cpp
#include "ipc/ipc.hpp"
```

## Компиляция

CMakeLists.txt должен включать:
```cmake
# Core
add_library(ipc_core
    src/core/cobs.cpp
)

# Transport layers
add_library(ipc_rs485
    src/transport/rs485/rs485_communication.cpp
    src/transport/rs485/rs485_adapter.cpp
    src/transport/rs485/packet_protocol.cpp
    src/transport/rs485/transport_layer.cpp
)

add_library(ipc_uart
    src/transport/uart/uart_communication.cpp
    src/transport/uart/uart_adapter.cpp
)

add_library(ipc_can
    src/transport/can/can_communication.cpp
    src/transport/can/can_adapter.cpp
)

# Выбрать нужный транспорт в target_link_libraries:
# ipc_rs485, ipc_uart, или ipc_can
```

## Миграция старого кода

Если у вас был старый код, использующий файлы из корня `include/`:

**Было:**
```cpp
#include "rs485_communication.hpp"
```

**Стало:**
```cpp
#include "ipc/transport/rs485/rs485_communication.hpp"
```

Старые файлы в корне `include/` оставлены для совместимости.
