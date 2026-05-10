// STM32 example: RS-485 with hidden service protocol and CRC16 + COBS framing.
// User code works with Messages; adapter controls DE/RE around transmission.

#include <rs485_communication.hpp>
#include <adapters/rs485_adapter.hpp>
#include <cstring>

extern UART_HandleTypeDef huart2;

static ipc::RS485Adapter rs485_adapter(&huart2);
static ipc::RS485Communication rs485(&rs485_adapter);

static size_t on_state_request(uint8_t requester,
                               const ipc::byte* request, size_t request_len,
                               ipc::byte* response, size_t response_max,
                               void*) {
    (void)requester;
    (void)request;
    (void)request_len;

    const ipc::byte state[] = {0x10, 0x20, 0x30};
    if (response_max < sizeof(state)) {
        return 0;
    }

    std::memcpy(response, state, sizeof(state));
    return sizeof(state);
}

static void on_message(const ipc::Message& msg, void*) {
    // Called for application messages addressed to this node or broadcast.
    (void)msg;
}

void app_rs485_init_slave() {
    ipc::RS485Adapter::Config adapter_cfg{};
    adapter_cfg.baudrate = 115200;
    adapter_cfg.de_mode = ipc::RS485Adapter::DEMode::kAutomatic;
    adapter_cfg.de_assert_delay = 10;
    adapter_cfg.de_deassert_delay = 10;
    rs485_adapter.init(adapter_cfg);

    ipc::RS485Communication::Config cfg{};
    cfg.device_id = 0x02;
    cfg.role = ipc::RS485Communication::Role::kSlave;
    cfg.baudrate = 115200;
    cfg.heartbeat_interval_ms = 5000;
    cfg.request_timeout_ms = 500;
    cfg.de_assert_delay = 10;
    cfg.de_deassert_delay = 10;

    rs485.init(cfg);
    rs485.set_message_callback(on_message, nullptr);
    rs485.set_state_request_handler(on_state_request, nullptr);
}

void app_rs485_poll(uint32_t now_ms) {
    rs485.poll(now_ms);
}

void app_rs485_send_command_to_master() {
    const ipc::byte payload[] = {0x42};
    ipc::Message msg = ipc::Message::create_command(0x02, 0x00, payload, sizeof(payload));
    rs485.send_message(msg);
}
