# Настройка подтверждений и информационных пакетов (ACK / Heartbeat / Info)

Этот документ показывает практические шаблоны, как **включать/выключать подтверждения и служебные/информационные пакеты** для `IPCLink`, `MasterNode` и `SlaveNode`.

> В текущей реализации ядра нет единого глобального флага вида `enable_ack` в `TransportConfig`.  
> Поэтому управление делается на уровне приложения: через callbacks, фильтрацию и условную отправку.

---

## 1) Базовая policy-структура приложения

```cpp
struct ServicePacketPolicy {
    bool send_ack = true;                 // отправлять подтверждения
    bool accept_ack = true;               // обрабатывать входящие ACK
    bool send_heartbeat = true;           // отправлять heartbeat
    bool accept_heartbeat = true;         // обрабатывать heartbeat
    bool send_unsolicited_state = true;   // слать инфо-пакеты State Update без запроса
    bool accept_unsolicited_state = true; // принимать такие инфо-пакеты
    bool send_error_packets = true;       // отправлять Service Error
};
```

Идея: policy хранится в вашем контексте, а в обработчиках вы проверяете флаги и решаете, слать/игнорировать пакет.

---

## 2) Пример: отключить ACK целиком

### 2.1 Не отправлять ACK

```cpp
if (policy.send_ack) {
    ipc::Packet ack = ipc::Packet::create_ack(dst_addr, seq_id);
    send_packet(ack);
}
```

### 2.2 Не обрабатывать входящие ACK

```cpp
void on_packet(const ipc::Packet& pkt) {
    if (pkt.header.type == ipc::PacketType::kService_Ack && !policy.accept_ack) {
        return; // ACK выключены в политике
    }

    // остальная обработка
}
```

---

## 3) Пример: отключить heartbeat (или только прием heartbeat)

### 3.1 Условная отправка heartbeat на slave

```cpp
void poll_slave(uint32_t now_ms) {
    if (!policy.send_heartbeat) {
        return;
    }

    // ваш код периодической отправки heartbeat
}
```

### 3.2 Игнорировать входящие heartbeat на master

```cpp
void on_master_packet(const ipc::Packet& pkt) {
    if (pkt.header.type == ipc::PacketType::kService_Heartbeat &&
        !policy.accept_heartbeat) {
        return;
    }

    // остальная обработка
}
```

---

## 4) Пример: отключить информационные пакеты State Update

Под информационными пакетами обычно понимают несолиситированные обновления (например `kState_Response`/`kState_Update` в вашей модели).

```cpp
bool send_state_update_guarded(ipc::SlaveNode& slave,
                               uint8_t state_id,
                               const uint8_t* data,
                               size_t len) {
    if (!policy.send_unsolicited_state) {
        return false; // выключено
    }
    return slave.send_state_update(state_id, data, len);
}
```

Фильтрация на приемнике:

```cpp
void on_packet_filtered(const ipc::Packet& pkt) {
    if (pkt.header.type == ipc::PacketType::kState_Response &&
        !policy.accept_unsolicited_state) {
        // можно добавить дополнительную проверку: есть ли pending request
        return;
    }
}
```

---

## 5) Готовые профили

### Профиль A: "Тихий CAN FD"
- ACK: off
- Heartbeat: off
- Unsolicited state: off
- Error packets: on

```cpp
ServicePacketPolicy can_quiet = {
    .send_ack = false,
    .accept_ack = false,
    .send_heartbeat = false,
    .accept_heartbeat = false,
    .send_unsolicited_state = false,
    .accept_unsolicited_state = false,
    .send_error_packets = true
};
```

### Профиль B: "Диагностика UART"
- ACK: on
- Heartbeat: on
- Unsolicited state: on
- Error packets: on

```cpp
ServicePacketPolicy uart_debug = {
    .send_ack = true,
    .accept_ack = true,
    .send_heartbeat = true,
    .accept_heartbeat = true,
    .send_unsolicited_state = true,
    .accept_unsolicited_state = true,
    .send_error_packets = true
};
```

---

## 6) Рекомендации

1. **CAN FD high-throughput**: обычно отключают heartbeat и лишние ACK, оставляя только критичные ошибки.
2. **UART/RS485 noisy channel**: ACK и heartbeat полезны для диагностики и контроля жизни узла.
3. Для production удобнее иметь единую `ServicePacketPolicy` в конфиге приложения и пробрасывать её в обработчики.

