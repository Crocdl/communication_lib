# Интеграция транспортного уровня в STM32 проект

## Структура кода

```
lib/ipc/include/
├── packet_protocol.hpp      # Определение пакетов протокола
├── transport_layer.hpp       # Master/Slave ноды
├── transport_adapter.hpp     # Базовый интерфейс
├── adapters/
│   ├── can_adapter.hpp       # CAN FD адаптер
│   ├── rs485_adapter.hpp     # RS485 адаптер
│   └── uart_adapter.hpp      # UART адаптер

lib/ipc/src/
├── packet_protocol.cpp       # Реализация пакетов
├── transport_layer.cpp       # Реализация Master/Slave
├── can_adapter.cpp           # Реализация CAN
├── rs485_adapter.cpp         # Реализация RS485
├── example_master_slave_integration.cpp  # Пример использования
```

## Типы пакетов

### Служебные пакеты
- `kService_Heartbeat` (0x01): Периодический сигнал жизни
- `kService_Ack` (0x02): Подтверждение
- `kService_Nak` (0x03): Отрицательное подтверждение
- `kService_Error` (0x04): Уведомление об ошибке

### Функциональные пакеты - Состояние
- `kState_Request` (0x10): Запрос состояния от master
- `kState_Response` (0x11): Ответ slave с состоянием
- `kState_Update` (0x12): Несолиситированное обновление от slave

### Функциональные пакеты - Команды
- `kCommand_Execute` (0x20): Master отправляет команду
- `kCommand_Response` (0x21): Slave отправляет результат
- `kCommand_Abort` (0x22): Master отменяет команду

## Быстрый старт

### 1. Конфигурация в CubeMX

Для **RS485**:
- Включить UART (например UART1)
- Настроить GPIO для DE/RE (например PA0 и PA1)
- Установить нужный baudrate

Для **CAN**:
- Включить CAN1 или CAN2
- Настроить GPIO для CAN TX/RX
- Включить CAN FD в параметрах

### 2. Инициализация в коде

**Пример для Slave на RS485:**
```cpp
// Инициализация адаптера
ipc::RS485Adapter adapter(&huart1);
ipc::RS485Adapter::Config rs485_cfg = {
    .baudrate = 115200,
    .de_mode = ipc::RS485Adapter::DEMode::kAutomatic,
    .de_assert_delay = 10,
    .de_deassert_delay = 10
};
adapter.init(rs485_cfg);

// Инициализация slave ноды
ipc::SlaveNode slave(&adapter);
ipc::TransportConfig cfg = {
    .node_address = 0x01,
    .is_master = false,
    .request_timeout_ms = 0,
    .heartbeat_interval_ms = 5000,
    .max_pending_requests = 0
};
slave.init(cfg);

// Регистрация обработчиков
slave.set_state_provider(my_state_provider, nullptr);
slave.set_command_handler(my_command_handler, nullptr);

// В главном цикле:
slave.poll(HAL_GetTick());
```

**Пример для Master на RS485:**
```cpp
// Аналогично для adapter.init()

// Инициализация master ноды
ipc::MasterNode master(&adapter);
ipc::TransportConfig cfg = {
    .node_address = 0x00,
    .is_master = true,
    .request_timeout_ms = 2000,
    .heartbeat_interval_ms = 0,
    .max_pending_requests = 16
};
master.init(cfg);

// Регистрация callback для ответов
master.set_packet_callback(my_response_handler, nullptr);

// В главном цикле:
// Отправить запрос:
master.request_state(0x01, 0);  // state_id=0 от slave 0x01
master.send_command(0x01, 0x10, cmd_data, cmd_len);

// Опросить ответы:
master.poll(HAL_GetTick());
```

## Обработчики для Slave

### State Provider
```cpp
size_t my_state_provider(uint8_t state_id, ipc::byte* buffer, 
                         size_t buffer_size, void* ctx) {
    switch (state_id) {
        case 0:
            // Заполнить буфер и вернуть размер
            memcpy(buffer, my_state_data, sizeof(my_state_data));
            return sizeof(my_state_data);
        default:
            return 0;
    }
}
```

### Command Handler
```cpp
ipc::ErrorCode my_command_handler(uint8_t cmd_id, const ipc::byte* cmd_data,
                                  size_t cmd_data_len, ipc::byte* response,
                                  size_t response_size, size_t* response_len, 
                                  void* ctx) {
    switch (cmd_id) {
        case 0x10:
            // Выполнить команду
            // Заполнить response и установить response_len
            *response_len = 2;
            return ipc::ErrorCode::kNoError;
        default:
            return ipc::ErrorCode::kCommandNotSupported;
    }
}
```

## Форматы пакетов

### Heartbeat (служебный)
```
| SOP | ADDR | TYPE | LEN   | SEQID | STATUS | UPTIME_MS[2] |
| 1B  | 1B   | 1B   | 2B(0) | 1B    | 1B     | 2B           |
```

### State Request
```
| SOP | ADDR | TYPE | LEN   | SEQID | STATE_ID |
| 1B  | 1B   | 1B   | 2B(1) | 1B    | 1B       |
```

### Command Execute
```
| SOP | ADDR | TYPE | LEN      | SEQID | CMD_ID | CMD_DATA[...] |
| 1B  | 1B   | 1B   | 2B(n)    | 1B    | 1B     | n-1 bytes     |
```

### Command Response
```
| SOP | ADDR | TYPE | LEN      | SEQID | CMD_ID | STATUS | RESP_DATA[...] |
| 1B  | 1B   | 1B   | 2B(n)    | 1B    | 1B     | 1B     | n-2 bytes      |
```

## Максимальные размеры

| Пакет | Max Data | Notes |
|-------|----------|-------|
| State Response | 248 B | 1B state_id + 247B data |
| Command Execute | 248 B | 1B cmd_id + 247B data |
| Command Response | 248 B | 1B cmd_id + 1B status + 246B data |
| Heartbeat | 3 B | fixed |

## Обработка ошибок

```cpp
enum class ErrorCode : uint8_t {
    kNoError = 0x00,
    kCRCError = 0x01,
    kProtocolError = 0x02,
    kCommandNotSupported = 0x03,
    kCommandFailed = 0x04,
    kTimeout = 0x05,
    kBufferOverflow = 0x06,
    kInvalidState = 0x07,
    kUnknownError = 0xFF,
};
```

## Отладка

Включите логирование через `logger.hpp`:
```cpp
LOG_ERROR("Error message");
LOG_WARN("Warning");
LOG_INFO("Information");
LOG_DEBUG("Debug info");
```

## Интеграция с кодеком

Пакеты автоматически сериализуются через:
1. **Packet** структура
2. **COBS** кодирование (для надежности)
3. **CRC16** проверка (целостность данных)
4. Отправка через **Transport Adapter** (RS485/CAN/UART)

## Тестирование на STM32

1. Создать две платы: Master и Slave
2. Подключить RS485 шину между ними
3. Загрузить `example_master_slave_integration.cpp` с флагом `DEVICE_IS_MASTER`
4. На Slave убрать флаг и загрузить
5. Наблюдать в Serial Monitor взаимодействие между устройствами

## Заметки по производительности

- Max throughput: ~1400 пакетов/сек на 115200 baud
- Latency: < 100 ms для request-response при 115200 baud
- Memory overhead: ~512B на adapter + ~256B на transport layer
- CAN FD: значительно выше throughput (~50Kbps+ в зависимости от конфигурации)
