# Протокол Транспортного Уровня RS485 Master-Slave

## Описание архитектуры

Протокол определяет иерархию «master-slave» для коммуникации по RS485 с поддержкой трёх типов сообщений:

### 1. Служебные пакеты (Service Packets)
Управляют коммуникацией и диагностикой:
- **Heartbeat (0x01)**: Периодический сигнал жизни от slave
- **Ack (0x02)**: Подтверждение получения
- **Nak (0x03)**: Отрицательное подтверждение
- **Error (0x04)**: Уведомление об ошибке

### 2. Функциональные пакеты - Состояние (State Packets)
Обмен состоянием устройства:
- **State Request (0x10)**: Master запрашивает состояние
- **State Response (0x11)**: Slave отправляет состояние
- **State Update (0x12)**: Slave самостоятельно отправляет обновление состояния

### 3. Функциональные пакеты - Команды (Command Packets)
Выполнение команд на slave:
- **Command Execute (0x20)**: Master отправляет команду
- **Command Response (0x21)**: Slave отправляет результат
- **Command Abort (0x22)**: Master отменяет команду

## Формат пакета

```
+--------+--------+--------+--------+--------+--------+---------+
|  SOP   | ADDR   | TYPE   | LEN    | SEQID  | DATA   | CRC16   |
+--------+--------+--------+--------+--------+--------+---------+
 0x7E    0-0x7E   uint8_t  uint16_t uint8_t  0-256B  2 bytes

SOP:    0x7E (Start of Packet marker)
ADDR:   Адрес slave (0x00 = broadcast, 0x01-0x7E = slave ID, 0x7F зарезервирован)
TYPE:   Тип пакета из enum PacketType
LEN:    Длина DATA в big-endian (сетевой порядок)
SEQID:  ID последовательности для связи запрос-ответ
DATA:   Полезная нагрузка (0-256 байт)
CRC16:  CRC16-CCITT (добавляется кодеком)
```

## Примеры использования

### Master Node

```cpp
#include "transport_layer.hpp"
#include "adapters/rs485_adapter.hpp"

// Инициализация
ipc::RS485Adapter rs485_adapter(huart);
ipc::MasterNode master(&rs485_adapter);

ipc::TransportConfig config = {
    .node_address = 0x00,              // Master имеет адрес 0x00
    .is_master = true,
    .request_timeout_ms = 1000,        // Timeout 1 сек
    .heartbeat_interval_ms = 5000,     // Не используется для master
    .max_pending_requests = 16
};

master.init(config);

// Регистрация callback для получения ответов
master.set_packet_callback([](const ipc::Packet& pkt, void* ctx) {
    switch (pkt.header.type) {
        case ipc::PacketType::kState_Response:
            // Обработка ответа на запрос состояния
            printf("Slave 0x%02X отправил состояние\n", pkt.header.address);
            break;
        case ipc::PacketType::kCommand_Response:
            // Обработка ответа на команду
            printf("Slave 0x%02X выполнил команду\n", pkt.header.address);
            break;
        case ipc::PacketType::kService_Heartbeat:
            // Получен heartbeat от slave
            printf("Heartbeat от slave 0x%02X\n", pkt.header.address);
            break;
        default:
            break;
    }
}, nullptr);

// В главном цикле:
while (1) {
    // Запросить состояние у slave
    if (some_condition) {
        master.request_state(0x01, 0);  // Запросить состояние 0 у slave 0x01
    }
    
    // Отправить команду
    if (other_condition) {
        uint8_t cmd_data[] = {0x01, 0x02, 0x03};
        master.send_command(0x01, 0x10, cmd_data, sizeof(cmd_data));
    }
    
    // Опросить timeouts и обработать ответы
    master.poll(HAL_GetTick());
    
    // Периодический опрос heartbeat
    if (time_check()) {
        master.broadcast_heartbeat_request();
    }
}
```

### Slave Node

```cpp
#include "transport_layer.hpp"
#include "adapters/rs485_adapter.hpp"

// Инициализация
ipc::RS485Adapter rs485_adapter(huart);
ipc::SlaveNode slave(&rs485_adapter);

ipc::TransportConfig config = {
    .node_address = 0x01,              // Slave ID = 0x01
    .is_master = false,
    .request_timeout_ms = 0,           // Не используется для slave
    .heartbeat_interval_ms = 5000,     // Отправлять heartbeat каждые 5 сек
    .max_pending_requests = 0
};

slave.init(config);

// Обработчик состояния - возвращает данные состояния
size_t state_provider(uint8_t state_id, ipc::byte* buffer, size_t buffer_size, void* ctx) {
    switch (state_id) {
        case 0: {
            // Состояние 0 - параметры устройства
            struct State {
                uint16_t voltage;
                uint16_t current;
                uint16_t temperature;
            } state = {3300, 500, 250};
            
            if (buffer_size < sizeof(state)) return 0;
            std::memcpy(buffer, &state, sizeof(state));
            return sizeof(state);
        }
        case 1: {
            // Состояние 1 - статус
            ipc::byte status = 0x42;
            buffer[0] = status;
            return 1;
        }
        case 0xFF:
            // Запрос всех состояний (пример: только состояние 0)
            return state_provider(0, buffer, buffer_size, ctx);
        default:
            return 0;
    }
}

// Обработчик команд
ipc::ErrorCode command_handler(uint8_t cmd_id, const ipc::byte* cmd_data,
                               size_t cmd_data_len, ipc::byte* response,
                               size_t response_size, size_t* response_len, void* ctx) {
    *response_len = 0;
    
    switch (cmd_id) {
        case 0x01:
            // Команда 1 - включить что-то
            if (cmd_data_len >= 1 && cmd_data[0] == 0x01) {
                // Do something...
                response[0] = 0x00;  // Success code
                *response_len = 1;
                return ipc::ErrorCode::kNoError;
            }
            return ipc::ErrorCode::kCommandFailed;
            
        case 0x02:
            // Команда 2 - выключить
            // Do something...
            return ipc::ErrorCode::kNoError;
            
        default:
            return ipc::ErrorCode::kCommandNotSupported;
    }
}

slave.set_state_provider(state_provider, nullptr);
slave.set_command_handler(command_handler, nullptr);

// Регистрация callback для отладки
slave.set_packet_callback([](const ipc::Packet& pkt, void* ctx) {
    printf("Slave отправил пакет типа 0x%02X\n", static_cast<uint8_t>(pkt.header.type));
}, nullptr);

// В главном цикле:
while (1) {
    // Обработать входящие команды и запросы, отправить heartbeat
    slave.poll(HAL_GetTick());
    
    // Опционально: отправить несолиситированное обновление состояния
    if (state_changed) {
        uint8_t state_buffer[] = {0x42, 0x43, 0x44};
        slave.send_state_update(0, state_buffer, sizeof(state_buffer));
        state_changed = false;
    }
}
```

## Диаграмма взаимодействия Master-Slave

### Запрос состояния
```
Master                                    Slave
  |                                         |
  | --- State Request (state_id=0) ------> |
  |     seq_id=0x01                        |
  |                                    [обработка]
  | <--- State Response (data,...) ------- |
  |     seq_id=0x01                        |
  |                                         |
```

### Отправка команды
```
Master                                    Slave
  |                                         |
  | --- Command Execute (cmd_id=0x10,...) |
  |     seq_id=0x02                        |
  |                                    [выполнение]
  | <--- Command Response (...) ---------- |
  |     seq_id=0x02, status                |
  |                                         |
```

### Периодический Heartbeat
```
Master                                    Slave
  |                                         |
  |                                    [каждые 5 сек]
  | <--- Heartbeat (status, uptime) ------ |
  |     seq_id=0x03                        |
  |                                         |
  | [проверка: все ли slave в сети]        |
  |                                         |
```

## Коды ошибок (ErrorCode)

| Код | Значение | Описание |
|-----|----------|---------|
| 0x00 | kNoError | Без ошибок |
| 0x01 | kCRCError | Ошибка CRC |
| 0x02 | kProtocolError | Ошибка протокола |
| 0x03 | kCommandNotSupported | Команда не поддерживается |
| 0x04 | kCommandFailed | Команда не выполнена |
| 0x05 | kTimeout | Timeout |
| 0x06 | kBufferOverflow | Переполнение буфера |
| 0x07 | kInvalidState | Недопустимое состояние |
| 0xFF | kUnknownError | Неизвестная ошибка |

## Максимальные размеры полезной нагрузки

- **State Request**: 1 байт (state_id)
- **State Response**: 248 байт (state_id + данные)
- **Command Execute**: 248 байт (cmd_id + данные)
- **Command Response**: 248 байт (cmd_id + status + данные)
- **Heartbeat**: 3 байта (status + uptime_ms)

## Интеграция с кодеком

Пакеты сериализуются через слой кодека (COBS + CRC16), который добавляет:
- COBS кодирование для надежности
- CRC16-CCITT для контроля целостности
- Маркер конца пакета (0x00)
