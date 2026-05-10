// STM32 example: CAN FD transport.
// CAN FD frames are sent without software CRC/COBS; large payloads are fragmented by IPCLink.

#include <can_communication.hpp>
#include <adapters/can_adapter.hpp>

extern CAN_HandleTypeDef hcan1;

static ipc::CANAdapter can_adapter(&hcan1);
static ipc::CANCommunication can_bus(&can_adapter);

static void on_message(const ipc::Message& msg, void*) {
    // Application receives complete reassembled payloads here.
    (void)msg;
}

void app_can_fd_init() {
    ipc::CANAdapter::CanFdConfig adapter_cfg{};
    adapter_cfg.nominal_bitrate = ipc::CANAdapter::BaudRate::kBaud500k;
    adapter_cfg.data_bitrate = ipc::CANAdapter::BaudRate::kBaud2M;
    adapter_cfg.enable_fd = true;
    adapter_cfg.enable_brs = true;
    adapter_cfg.enable_esi = false;
    adapter_cfg.std_filter_mask = 0;
    adapter_cfg.ext_filter_mask = 0;
    can_adapter.init(adapter_cfg);

    ipc::CANCommunication::Config cfg{};
    cfg.device_id = 0x11;
    cfg.nominal_bitrate = ipc::CANCommunication::BaudRate::kBaud500k;
    cfg.data_bitrate = ipc::CANCommunication::BaudRate::kBaud2M;
    cfg.enable_fd = true;
    cfg.enable_brs = true;
    cfg.receive_timeout_ms = 100;

    can_bus.init(cfg);
    can_bus.set_message_callback(on_message, nullptr);
}

void app_can_fd_poll(uint32_t now_ms) {
    can_bus.poll(now_ms);
}

void app_can_fd_send_state() {
    ipc::byte state[80] = {};
    for (size_t i = 0; i < sizeof(state); ++i) {
        state[i] = static_cast<ipc::byte>(i);
    }

    ipc::Message msg = ipc::Message::create_state_update(0x11, 0x22, state, sizeof(state));
    can_bus.send_message(msg);
}
