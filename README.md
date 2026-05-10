# communication_lib

Обновлённая документация сфокусирована на практическом использовании через `UART`, полном пути прохождения сообщения и формате пакетов.

## Что внутри

- [UART_USAGE.md](UART_USAGE.md) — пошаговый пример интеграции и обмена сообщениями через `UARTCommunication`.
- [PACKET_FORMATS.md](PACKET_FORMATS.md) — общий формат пакетов, формат на уровне `IPCLink` и конкретные примеры байт.
- `examples/uart_basic.cpp` — пример UART для ESP32/Arduino.
- `examples/stm32_uart_hal_basic.cpp` — пример UART через STM32 HAL.
- `examples/can_fd_basic.cpp` — пример CAN FD для STM32.
- `examples/rs485_basic.cpp` — пример RS-485 master/slave-слоя для STM32.

## Надежность и буферы

- UART и RS-485 по умолчанию используют `CRC16_CCITT` и `COBS`-фрейминг.
- CAN FD использует компактный length-prefix без программного CRC/COBS; длинные сообщения автоматически фрагментируются на физические кадры до 64 байт и собираются обратно.
- `IPCLink` содержит TX/RX-очереди, `send_async`, `receive`, `pending_tx`, `available_rx` и расширенную статистику ошибок, байт и фрагментов.
- Для native-тестов есть виртуальный `MockTransport`, который позволяет прогонять loopback без железа.

## Быстрый старт (UART)

1. Создайте `UARTAdapter` и запустите порт.
2. Создайте `UARTCommunication`, передайте адаптер и вызовите `init`.
3. Подпишитесь на callback и handlers.
4. Отправляйте `Message` через `send_message`.
5. В главном цикле регулярно вызывайте `poll`.

Полный рабочий пример — в `UART_USAGE.md`.
