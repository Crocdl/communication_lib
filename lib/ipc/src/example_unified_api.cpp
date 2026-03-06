/**
 * @file example_unified_api.cpp
 * @brief Example: Unified Communication API for all transports
 * 
 * Demonstrates how to use the same API for RS485, CAN, and UART
 * Upper level doesn't care about transport or master-slave hierarchy
 */

#include "communication_layer.hpp"
#include "rs485_communication.hpp"
#include "can_communication.hpp"
#include "uart_communication.hpp"
#include "adapters/rs485_adapter.hpp"
#include "adapters/can_adapter.hpp"
#include "logger.hpp"

/**
 * ============================================================================
 * Generic Device Class (works with any transport)
 * ============================================================================
 */

class DeviceNode {
public:
    explicit DeviceNode(ipc::CommunicationLayer* comm) 
        : comm_(comm) {}

    bool initialize() {
        if (!comm_ || !comm_->is_ready()) {
            LOG_ERROR("Communication not ready");
            return false;
        }

        // Register callbacks (same for any transport)
        comm_->set_message_callback([](const ipc::Message& msg, void* ctx) {
            static_cast<DeviceNode*>(ctx)->on_message_received(msg);
        }, this);

        comm_->set_state_request_handler([](uint8_t src_id, const uint8_t* req_data, size_t req_len,
                                          uint8_t* resp_data, size_t resp_max, void* ctx) {
            return static_cast<DeviceNode*>(ctx)->handle_state_request(src_id, req_data, req_len,
                                                                      resp_data, resp_max);
        }, this);

        comm_->set_command_handler([](uint8_t src_id, const uint8_t* cmd_data, size_t cmd_len,
                                     uint8_t* resp_data, size_t resp_max, size_t* resp_len, void* ctx) {
            return static_cast<DeviceNode*>(ctx)->handle_command(src_id, cmd_data, cmd_len,
                                                               resp_data, resp_max, resp_len);
        }, this);

        LOG_INFO("Device 0x%02X initialized on transport", comm_->get_device_id());
        return true;
    }

    void send_state_update(uint8_t dest_id, const uint8_t* data, size_t len) {
        ipc::Message msg = ipc::Message::create_state_update(comm_->get_device_id(), dest_id, data, len);
        comm_->send_message(msg);
    }

    void send_command(uint8_t dest_id, const uint8_t* cmd_data, size_t cmd_len) {
        ipc::Message msg = ipc::Message::create_command(comm_->get_device_id(), dest_id, cmd_data, cmd_len);
        comm_->send_message(msg);
    }

    void request_state(uint8_t dest_id, const uint8_t* req_data = nullptr, size_t req_len = 0) {
        ipc::Message msg = ipc::Message::create_state_request(comm_->get_device_id(), dest_id, req_data, req_len);
        comm_->send_message(msg);
    }

    void poll(uint32_t now) {
        comm_->poll(now);
    }

private:
    ipc::CommunicationLayer* comm_;

    void on_message_received(const ipc::Message& msg) {
        switch (msg.type) {
            case ipc::MessageType::kState_Update:
                LOG_INFO("Received state update from 0x%02X, size=%u", msg.source_id, msg.payload_size);
                break;
            case ipc::MessageType::kState_Request:
                LOG_INFO("Received state request from 0x%02X", msg.source_id);
                break;
            case ipc::MessageType::kCommand_Execute:
                LOG_INFO("Received command from 0x%02X", msg.source_id);
                break;
            case ipc::MessageType::kCommand_Response:
                LOG_INFO("Received command response from 0x%02X", msg.source_id);
                break;
        }
    }

    size_t handle_state_request(uint8_t src_id, const uint8_t* req_data, size_t req_len,
                               uint8_t* resp_data, size_t resp_max) {
        LOG_INFO("Handling state request from 0x%02X", src_id);
        
        // Example: return device state
        if (resp_max >= 4) {
            resp_data[0] = 0x12;  // Sensor reading 1
            resp_data[1] = 0x34;  // Sensor reading 2
            resp_data[2] = 0x56;  // Status
            resp_data[3] = 0x78;  // Timestamp
            return 4;
        }
        return 0;
    }

    ipc::ErrorCode handle_command(uint8_t src_id, const uint8_t* cmd_data, size_t cmd_len,
                                 uint8_t* resp_data, size_t resp_max, size_t* resp_len) {
        LOG_INFO("Handling command from 0x%02X, cmd_len=%u", src_id, cmd_len);
        
        if (cmd_len < 1) {
            return ipc::ErrorCode::kProtocolError;
        }

        uint8_t cmd_id = cmd_data[0];
        *resp_len = 0;

        switch (cmd_id) {
            case 0x01:  // Example command: get info
                if (resp_max >= 3) {
                    resp_data[0] = 0x01;  // Version
                    resp_data[1] = 0x02;  // Model
                    resp_data[2] = 0x00;  // Revision
                    *resp_len = 3;
                    return ipc::ErrorCode::kNoError;
                }
                return ipc::ErrorCode::kBufferOverflow;

            case 0x02:  // Example command: reset
                LOG_INFO("Command: Reset device");
                *resp_len = 1;
                resp_data[0] = 0x00;  // Status OK
                return ipc::ErrorCode::kNoError;

            default:
                return ipc::ErrorCode::kCommandNotSupported;
        }
    }
};

/**
 * ============================================================================
 * EXAMPLE 1: RS485 (Master-Slave, but API is same for all devices)
 * ============================================================================
 */

void example_rs485() {
    LOG_INFO("=== RS485 Example ===");
    
    // Create RS485 adapter
    ipc::RS485Adapter adapter(&huart1);
    ipc::RS485Adapter::Config adapter_cfg = {
        115200, ipc::RS485Adapter::DEMode::kAutomatic, 10, 10
    };
    adapter.init(adapter_cfg);

    // Create RS485 communication layer
    ipc::RS485Communication comm(&adapter);
    ipc::RS485Communication::Config cfg = {
        0x01,                        // device_id (0x00 = master, 0x01-0x7E = slave)
        ipc::RS485Communication::Role::kSlave,
        115200,
        5000,                        // heartbeat_interval_ms (for slaves)
        2000,                        // request_timeout_ms (for master)
        10,                          // de_assert_delay
        10                           // de_deassert_delay
    };
    comm.init(cfg);

    // Create device node (same code works for master and slave)
    DeviceNode device(&comm);
    device.initialize();

    // Main loop - same API for any role
    while (1) {
        device.request_state(0x00, nullptr, 0);  // Request from master/peer
        device.poll(HAL_GetTick());
        HAL_Delay(1000);
    }
}

/**
 * ============================================================================
 * EXAMPLE 2: CAN (All devices peer-to-peer)
 * ============================================================================
 */

void example_can() {
    LOG_INFO("=== CAN Example ===");
    
    // Create CAN adapter
    ipc::CANAdapter can_adapter(&hcan1);
    ipc::CANAdapter::CanFdConfig adapter_cfg = {
        ipc::CANAdapter::BaudRate::kBaud500k,
        ipc::CANAdapter::BaudRate::kBaud2M,
        true,   // enable_fd
        true,   // enable_brs
        false   // enable_esi
    };
    can_adapter.init(adapter_cfg);

    // Create CAN communication layer
    ipc::CANCommunication comm(&can_adapter);
    ipc::CANCommunication::Config cfg = {
        0x05,                        // device_id (any device is independent)
        ipc::CANCommunication::BaudRate::kBaud500k,
        ipc::CANCommunication::BaudRate::kBaud2M,
        true,   // enable_fd
        true    // enable_brs
    };
    comm.init(cfg);

    // Create device node (same code as RS485)
    DeviceNode device(&comm);
    device.initialize();

    // Main loop - SAME API
    while (1) {
        device.request_state(0xFF, nullptr, 0);  // Broadcast request
        device.poll(HAL_GetTick());
        HAL_Delay(1000);
    }
}

/**
 * ============================================================================
 * EXAMPLE 3: UART (All devices point-to-point)
 * ============================================================================
 */

void example_uart() {
    LOG_INFO("=== UART Example ===");
    
    // Create UART adapter
    ipc::UARTAdapter uart_adapter(&huart2);
    
    // Create UART communication layer
    ipc::UARTCommunication comm(&uart_adapter);
    ipc::UARTCommunication::Config cfg = {
        0x01,     // device_id
        115200    // baudrate
    };
    comm.init(cfg);

    // Create device node (same code as all others)
    DeviceNode device(&comm);
    device.initialize();

    // Main loop - SAME API
    uint32_t last_cmd = 0;
    while (1) {
        uint32_t now = HAL_GetTick();
        
        if (now - last_cmd > 2000) {
            last_cmd = now;
            device.send_command(0x02, (const uint8_t*)"\x01", 1);  // Send command 0x01
        }

        device.poll(now);
        HAL_Delay(10);
    }
}

/**
 * ============================================================================
 * MAIN - Choose one example
 * ============================================================================
 */

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_CAN1_Init();

    // Choose which transport to use:
    
    // example_rs485();   // Master-Slave on RS485
    // example_can();     // Peer-to-peer on CAN
    example_uart();    // Point-to-point on UART

    return 0;
}

/**
 * ============================================================================
 * KEY POINTS:
 * ============================================================================
 * 
 * 1. Upper-level API (DeviceNode) is IDENTICAL for all transports
 * 
 * 2. User logic (send_state_update, send_command, etc) is SAME
 * 
 * 3. Service protocols (master-slave, arbitration, etc) are HIDDEN inside
 *    CommunicationLayer implementations
 * 
 * 4. Message format and error codes are UNIFORM across all transports
 * 
 * 5. Handlers (state_request, command_handler) use same signatures
 * 
 * 6. To switch transport: just change which CommunicationLayer you instantiate
 * 
 * ============================================================================
 */
