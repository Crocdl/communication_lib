# Быстрый старт: Master-Slave на RS485

## 1️⃣ Минимальная инициализация Slave

```cpp
#include "transport_layer.hpp"
#include "adapters/rs485_adapter.hpp"

// Глобальные переменные
ipc::RS485Adapter* g_rs485;
ipc::SlaveNode* g_slave;

// Handler состояния
size_t state_provider(uint8_t state_id, ipc::byte* buf, size_t len, void* ctx) {
    if (state_id == 0 && len >= 4) {
        buf[0] = 0x12; buf[1] = 0x34;  // Напряжение 0x1234
        buf[2] = 0x56; buf[3] = 0x78;  // Ток 0x5678
        return 4;
    }
    return 0;
}

// Handler команд
ipc::ErrorCode cmd_handler(uint8_t cmd_id, const ipc::byte* data, size_t len,
                           ipc::byte* resp, size_t resp_len, size_t* resp_size, void* ctx) {
    if (cmd_id == 0x10) {
        *resp_size = 2;
        resp[0] = 0xAA; resp[1] = 0xBB;
        return ipc::ErrorCode::kNoError;
    }
    return ipc::ErrorCode::kCommandNotSupported;
}

// В main():
void setup_slave() {
    // Инициализация RS485
    g_rs485 = new ipc::RS485Adapter(&huart1);
    ipc::RS485Adapter::Config cfg = {115200, ipc::RS485Adapter::DEMode::kAutomatic, 10, 10};
    g_rs485->init(cfg);
    
    // Инициализация Slave
    g_slave = new ipc::SlaveNode(g_rs485);
    ipc::TransportConfig tcfg = {0x01, false, 0, 5000, 0};
    g_slave->init(tcfg);
    g_slave->set_state_provider(state_provider, nullptr);
    g_slave->set_command_handler(cmd_handler, nullptr);
}

// В главном цикле:
void loop() {
    g_slave->poll(HAL_GetTick());
}
```

## 2️⃣ Минимальная инициализация Master

```cpp
#include "transport_layer.hpp"
#include "adapters/rs485_adapter.hpp"

ipc::RS485Adapter* g_rs485;
ipc::MasterNode* g_master;
uint32_t g_last_cmd_time = 0;

// Handler ответов
void packet_handler(const ipc::Packet& pkt, void* ctx) {
    if (pkt.header.type == ipc::PacketType::kState_Response) {
        printf("State: 0x%02X 0x%02X\n", pkt.payload[1], pkt.payload[2]);
    }
}

// В main():
void setup_master() {
    g_rs485 = new ipc::RS485Adapter(&huart1);
    ipc::RS485Adapter::Config cfg = {115200, ipc::RS485Adapter::DEMode::kAutomatic, 10, 10};
    g_rs485->init(cfg);
    
    g_master = new ipc::MasterNode(g_rs485);
    ipc::TransportConfig tcfg = {0x00, true, 2000, 0, 16};
    g_master->init(tcfg);
    g_master->set_packet_callback(packet_handler, nullptr);
}

// В главном цикле:
void loop() {
    if (HAL_GetTick() - g_last_cmd_time > 1000) {
        g_last_cmd_time = HAL_GetTick();
        g_master->request_state(0x01, 0);  // Запрос состояния
    }
    g_master->poll(HAL_GetTick());
}
```

## 3️⃣ Таблица типов пакетов

| Тип | ID | Описание | Данные |
|-----|----|---------|----|
| Heartbeat | 0x01 | Status, Uptime | 3B |
| Ack | 0x02 | SeqID | 1B |
| Error | 0x04 | ErrorCode, Info | 2B |
| State Request | 0x10 | StateID | 1B |
| State Response | 0x11 | StateID + Data | до 248B |
| Command | 0x20 | CmdID + Data | до 248B |
| Command Response | 0x21 | CmdID + Status + Data | до 248B |

## 4️⃣ Коды ошибок

```cpp
kNoError = 0x00
kCRCError = 0x01
kCommandNotSupported = 0x03
kCommandFailed = 0x04
kTimeout = 0x05
kBufferOverflow = 0x06
```

## 5️⃣ Типичные операции

### Запрос состояния с timeout
```cpp
master.request_state(0x01, 0);  // slave=0x01, state=0
// Ждём в loop() ответ в packet_handler с seq_id
```

### Отправка команды
```cpp
uint8_t cmd_data[] = {0x01, 0x02};
master.send_command(0x01, 0x10, cmd_data, 2);
// Ответ приходит в packet_handler
```

### Отправка состояния от slave
```cpp
uint8_t state[] = {0xAA, 0xBB, 0xCC};
slave.send_state_update(0, state, 3);  // state_id=0, данные
```

## 6️⃣ Конфигурация UART в CubeMX

1. Выбрать UART (USART1)
2. Mode: Asynchronous
3. Baudrate: 115200
4. Добавить GPIO для DE/RE:
   - PA0 → GPIO_Output (для DE)
   - PA1 → GPIO_Output (для RE)
5. Добавить interrupt USART1_IRQHandler

## 7️⃣ Таймы и limits

| Параметр | Значение | Описание |
|----------|----------|---------|
| Max State Data | 247 B | State Response payload |
| Max Command Data | 247 B | Command Execute payload |
| Max Slave Nodes | 126 | Адреса 0x01-0x7E |
| State Request Timeout | 1-2 сек | На master |
| Heartbeat Interval | 5 сек | На slave |
| Max Pending Requests | 16 | На master |

## 8️⃣ Отладка - включить LOG

В `logger.hpp` или main:
```cpp
#define LOG_LEVEL LOG_DEBUG  // или LOG_INFO

LOG_INFO("Инициализация");
LOG_ERROR("Ошибка!");
```

## 9️⃣ Структура пакета в эфире

```
[0x7E] [ADDR] [TYPE] [LEN_H] [LEN_L] [SEQ] [PAYLOAD...] [CRC_H] [CRC_L] [0x00]
  SOP   Адрес  Тип   Длина            ID    Данные         Контроль     Маркер
```

## 🔟 Типичный flow State Request

```
Master → Slave
  Пакет: [0x7E][0x01][0x10][0x00][0x01][0x00][state_id=0][CRC][0x00]
                      └─ State Request

Slave → Master
  Пакет: [0x7E][0x00][0x11][0x00][0x04][0x00][state_id=0][data...][CRC][0x00]
                      └─ State Response
```

## 🚀 Быстрый тест

```bash
# Компилировать
cd /path/to/project
cmake build/
make

# Загрузить на плату
openocd -f board.cfg
# в gdb: load, continue

# Мониторить Serial
minicom -D /dev/ttyUSB0 -b 115200
```

---

**Готово к использованию!** Скопируй код выше в `main.cpp` и адаптируй под свой проект.
