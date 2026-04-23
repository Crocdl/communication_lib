# Формат пакетов и путь сообщения

Документ описывает:
1) путь сообщения от приложения до UART,
2) общий формат полезной нагрузки уровня `CommunicationLayer`,
3) как это выглядит на линии после `IPCLink`,
4) конкретные примеры в hex.

---

## 1. Путь сообщения (Message Path)

```text
Application code
  -> Message::create_*()
  -> CommunicationLayer::send_message()
  -> serialize в raw payload:
       [SRC][DST][TYPE][LEN_H][LEN_L][PAYLOAD...]
  -> IPCLink::send()
       + CRC (если включен)
       + COBS (если включен)
  -> ITransportAdapter::send()
  -> UART TX
```

Обратный путь:

```text
UART RX
  -> ITransportAdapter RX callback
  -> IPCLink::on_raw_byte()
       COBS decode
       CRC validate
  -> raw payload:
       [SRC][DST][TYPE][LEN_H][LEN_L][PAYLOAD...]
  -> CommunicationLayer decode в Message
  -> message callback / state handler / command handler
```

---

## 2. Общий формат payload между CommunicationLayer и IPCLink

```text
Byte 0: SRC_ID
Byte 1: DST_ID
Byte 2: TYPE
Byte 3: LEN_H
Byte 4: LEN_L
Byte 5..N: PAYLOAD
```

Где:
- `SRC_ID` — адрес отправителя.
- `DST_ID` — адрес получателя (`0xFF` = broadcast).
- `TYPE` — тип сообщения (`kState_Update`, `kState_Request`, `kCommand_Execute`, `kCommand_Response`).
- `LEN` — длина `PAYLOAD` (big-endian, `LEN_H << 8 | LEN_L`).

---

## 3. Формат на линии UART (с IPCLink в режиме UART)

Для UART используется конфигурация `IPCLink::Config::for_uart()`:
- `use_cobs = true`
- `use_crc = true`
- `use_length_prefix = false`

То есть фактический кадр на линии:

```text
COBS_ENCODE( [SRC][DST][TYPE][LEN_H][LEN_L][PAYLOAD...][CRC16] )
```

---

## 4. Конкретные примеры

### Пример A: Command "PING" от 0x01 к 0x02

Исходный `Message`:
- `source_id = 0x01`
- `dest_id = 0x02`
- `type = 0x20` (`kCommand_Execute`)
- `payload = 50 49 4E 47` (`"PING"`)
- `payload_size = 0x0004`

Raw payload (до IPCLink codec):

```text
01 02 20 00 04 50 49 4E 47
```

Дальше `IPCLink` добавляет CRC16 и COBS-кодирует кадр.

### Пример B: Broadcast state update от 0x10

Исходный `Message`:
- `source_id = 0x10`
- `dest_id = 0xFF`
- `type = 0x10` (`kState_Update`)
- `payload = AA BB`
- `payload_size = 0x0002`

Raw payload:

```text
10 FF 10 00 02 AA BB
```

Дальше аналогично: `+CRC16`, затем `COBS`.

---

## 5. Ключевые проверки при декодировании

При разборе входящего raw payload проверяется:
1. минимальная длина (`>= 5`),
2. корректность длины (`payload_len + 5 == frame_len`),
3. ограничение размера payload (не больше `Message::kMaxPayloadSize`).

Если проверка не проходит — пакет отбрасывается.
