// STM32 HAL example: UART + IPC UARTCommunication.
// CubeMX should initialize huart2 and GPIO clocks before app_uart_init().

#include <uart_communication.hpp>
#include <adapters/stm32_uart_adapter.hpp>
#include <cstring>

extern UART_HandleTypeDef huart2;

static ipc::STM32UARTAdapter uart_adapter(&huart2);
static ipc::UARTCommunication uart_comm(&uart_adapter);

static void on_uart_message(const ipc::Message& msg, void*) {
    // Application receives complete, CRC-checked payloads here.
    // msg.source_id, msg.dest_id, msg.type and msg.payload are ready to use.
    (void)msg;
}

static ipc::ErrorCode on_command(uint8_t source_id,
                                 const ipc::byte* cmd, size_t cmd_len,
                                 ipc::byte* response, size_t response_max,
                                 size_t* response_len,
                                 void*) {
    (void)source_id;

    if (!cmd || cmd_len == 0 || response_max < 1) {
        *response_len = 0;
        return ipc::ErrorCode::kProtocolError;
    }

    response[0] = cmd[0];
    *response_len = 1;
    return ipc::ErrorCode::kNoError;
}

void app_uart_init() {
    ipc::STM32UARTAdapter::Config adapter_cfg;
    adapter_cfg.baudrate = 115200;
    adapter_cfg.register_hal_callbacks = false;
    adapter_cfg.buffer_rx_in_interrupt = true;
    uart_adapter.init(adapter_cfg);

    ipc::UARTCommunication::Config comm_cfg{};
    comm_cfg.device_id = 0x01;
    comm_cfg.baudrate = adapter_cfg.baudrate;
    uart_comm.init(comm_cfg);
    uart_comm.set_message_callback(on_uart_message, nullptr);
    uart_comm.set_command_handler(on_command, nullptr);
}

void app_uart_poll(uint32_t now_ms) {
    uart_comm.poll(now_ms);
}

void app_uart_send_state() {
    const ipc::byte payload[] = {0x10, 0x20, 0x30, 0x40};
    ipc::Message msg = ipc::Message::create_state_update(0x01, 0x02, payload, sizeof(payload));
    uart_comm.send_message(msg);
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    ipc::STM32UARTAdapter::handle_rx_complete(huart);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    ipc::STM32UARTAdapter::handle_tx_complete(huart);
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    ipc::STM32UARTAdapter::handle_error(huart);
}
