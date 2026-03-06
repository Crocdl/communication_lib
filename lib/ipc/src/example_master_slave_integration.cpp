/**
 * @file example_master_slave_integration.cpp
 * @brief Example of Master-Slave communication using RS485 transport layer
 * 
 * This example demonstrates how to integrate the transport layer with
 * RS485 adapter for complete master-slave communication.
 */

#include "transport_layer.hpp"
#include "adapters/rs485_adapter.hpp"
#include "codec.hpp"
#include "logger.hpp"

/**
 * ============================================================================
 * MASTER NODE EXAMPLE
 * ============================================================================
 */

class MasterApplication {
public:
    MasterApplication(ipc::ITransportAdapter* adapter) 
        : master_(adapter), adapter_(adapter) {}

    bool initialize() {
        // Configure RS485 adapter
        ipc::RS485Adapter::Config rs485_config = {
            .baudrate = 115200,
            .de_mode = ipc::RS485Adapter::DEMode::kAutomatic,
            .de_assert_delay = 10,      // 10 µs
            .de_deassert_delay = 10     // 10 µs
        };

        auto* rs485 = dynamic_cast<ipc::RS485Adapter*>(adapter_);
        if (rs485 && !rs485->init(rs485_config)) {
            LOG_ERROR("Failed to initialize RS485 adapter");
            return false;
        }

        // Configure and initialize master node
        ipc::TransportConfig transport_config = {
            .node_address = 0x00,           // Master address
            .is_master = true,
            .request_timeout_ms = 2000,     // 2 second timeout
            .heartbeat_interval_ms = 0,     // Not used for master
            .max_pending_requests = 16
        };

        if (!master_.init(transport_config)) {
            LOG_ERROR("Failed to initialize master node");
            return false;
        }

        // Register callback for incoming packets
        master_.set_packet_callback([](const ipc::Packet& pkt, void* ctx) {
            MasterApplication* app = static_cast<MasterApplication*>(ctx);
            app->handle_packet_received(pkt);
        }, this);

        LOG_INFO("Master application initialized");
        return true;
    }

    void run_main_loop() {
        uint32_t state_request_timer = 0;
        uint32_t cmd_timer = 0;

        while (1) {
            uint32_t now = HAL_GetTick();

            // Request state from slave every 1 second
            if (now - state_request_timer >= 1000) {
                state_request_timer = now;
                
                // Request state 0 from slave 0x01
                if (!master_.request_state(0x01, 0)) {
                    LOG_ERROR("Failed to send state request");
                }
            }

            // Send command every 3 seconds
            if (now - cmd_timer >= 3000) {
                cmd_timer = now;
                
                // Send command 0x10 with data
                uint8_t cmd_data[] = {0xAA, 0xBB, 0xCC};
                if (!master_.send_command(0x01, 0x10, cmd_data, sizeof(cmd_data))) {
                    LOG_ERROR("Failed to send command");
                }
            }

            // Poll for responses and timeout checks
            master_.poll(now);

            // Small delay to avoid busy loop
            HAL_Delay(10);
        }
    }

private:
    ipc::MasterNode master_;
    ipc::ITransportAdapter* adapter_;

    void handle_packet_received(const ipc::Packet& pkt) {
        switch (pkt.header.type) {
            case ipc::PacketType::kState_Response: {
                LOG_INFO("State Response from slave 0x%02X:", pkt.header.address);
                if (pkt.payload_size > 1) {
                    uint8_t state_id = pkt.payload[0];
                    LOG_INFO("  State ID: 0x%02X, Data length: %u", 
                            state_id, pkt.payload_size - 1);
                    // Process state data...
                }
                break;
            }
            case ipc::PacketType::kCommand_Response: {
                LOG_INFO("Command Response from slave 0x%02X:", pkt.header.address);
                if (pkt.payload_size > 1) {
                    uint8_t cmd_id = pkt.payload[0];
                    uint8_t status = pkt.payload[1];
                    LOG_INFO("  Command ID: 0x%02X, Status: 0x%02X", cmd_id, status);
                }
                break;
            }
            case ipc::PacketType::kService_Heartbeat: {
                LOG_INFO("Heartbeat from slave 0x%02X", pkt.header.address);
                if (pkt.payload_size >= 1) {
                    uint8_t slave_status = pkt.payload[0];
                    LOG_INFO("  Slave status: 0x%02X", slave_status);
                }
                break;
            }
            case ipc::PacketType::kService_Error: {
                LOG_ERROR("Error from slave 0x%02X", pkt.header.address);
                if (pkt.payload_size >= 1) {
                    uint8_t error_code = pkt.payload[0];
                    LOG_ERROR("  Error code: 0x%02X", error_code);
                }
                break;
            }
            default:
                LOG_WARN("Unknown packet type: 0x%02X", static_cast<uint8_t>(pkt.header.type));
                break;
        }
    }
};

/**
 * ============================================================================
 * SLAVE NODE EXAMPLE
 * ============================================================================
 */

class SlaveApplication {
public:
    SlaveApplication(ipc::ITransportAdapter* adapter) 
        : slave_(adapter), adapter_(adapter) {}

    bool initialize(uint8_t slave_address) {
        // Configure RS485 adapter
        ipc::RS485Adapter::Config rs485_config = {
            .baudrate = 115200,
            .de_mode = ipc::RS485Adapter::DEMode::kAutomatic,
            .de_assert_delay = 10,
            .de_deassert_delay = 10
        };

        auto* rs485 = dynamic_cast<ipc::RS485Adapter*>(adapter_);
        if (rs485 && !rs485->init(rs485_config)) {
            LOG_ERROR("Failed to initialize RS485 adapter");
            return false;
        }

        // Configure and initialize slave node
        ipc::TransportConfig transport_config = {
            .node_address = slave_address,
            .is_master = false,
            .request_timeout_ms = 0,
            .heartbeat_interval_ms = 5000,  // Send heartbeat every 5 seconds
            .max_pending_requests = 0
        };

        if (!slave_.init(transport_config)) {
            LOG_ERROR("Failed to initialize slave node");
            return false;
        }

        // Register state provider
        slave_.set_state_provider(state_provider_static, this);

        // Register command handler
        slave_.set_command_handler(command_handler_static, this);

        LOG_INFO("Slave application initialized with address 0x%02X", slave_address);
        return true;
    }

    void run_main_loop() {
        while (1) {
            uint32_t now = HAL_GetTick();

            // Poll for incoming requests and send heartbeats
            slave_.poll(now);

            // Send unsolicited state update every 10 seconds if state changed
            if (state_changed_) {
                update_state();
                state_changed_ = false;
            }

            HAL_Delay(10);
        }
    }

private:
    ipc::SlaveNode slave_;
    ipc::ITransportAdapter* adapter_;
    bool state_changed_ = false;

    // State data structure
    struct DeviceState {
        uint16_t voltage;
        uint16_t current;
        uint16_t temperature;
    } device_state_ = {3300, 500, 250};

    // Static wrappers for callback functions
    static size_t state_provider_static(uint8_t state_id, ipc::byte* buffer, 
                                        size_t buffer_size, void* ctx) {
        SlaveApplication* app = static_cast<SlaveApplication*>(ctx);
        return app->provide_state(state_id, buffer, buffer_size);
    }

    static ipc::ErrorCode command_handler_static(uint8_t cmd_id, const ipc::byte* cmd_data,
                                                 size_t cmd_data_len, ipc::byte* response,
                                                 size_t response_size, size_t* response_len, void* ctx) {
        SlaveApplication* app = static_cast<SlaveApplication*>(ctx);
        return app->handle_command(cmd_id, cmd_data, cmd_data_len, response, response_size, response_len);
    }

    // Implementation of state provider
    size_t provide_state(uint8_t state_id, ipc::byte* buffer, size_t buffer_size) {
        switch (state_id) {
            case 0: {
                // Return device state
                if (buffer_size < sizeof(device_state_)) {
                    return 0;
                }
                std::memcpy(buffer, &device_state_, sizeof(device_state_));
                LOG_DEBUG("Provided state 0 (voltage=%u, current=%u, temp=%u)",
                         device_state_.voltage, device_state_.current, device_state_.temperature);
                return sizeof(device_state_);
            }
            case 1: {
                // Return status flags
                if (buffer_size < 1) {
                    return 0;
                }
                buffer[0] = 0x01;  // Device OK
                LOG_DEBUG("Provided state 1 (status flags)");
                return 1;
            }
            case 0xFF: {
                // Return all states (example: just state 0)
                return provide_state(0, buffer, buffer_size);
            }
            default:
                LOG_WARN("Unknown state requested: 0x%02X", state_id);
                return 0;
        }
    }

    // Implementation of command handler
    ipc::ErrorCode handle_command(uint8_t cmd_id, const ipc::byte* cmd_data, size_t cmd_data_len,
                                  ipc::byte* response, size_t response_size, size_t* response_len) {
        LOG_INFO("Handling command 0x%02X with %u bytes of data", cmd_id, cmd_data_len);

        switch (cmd_id) {
            case 0x10: {
                // Example command: Set voltage
                if (cmd_data_len >= 2) {
                    uint16_t new_voltage = (cmd_data[0] << 8) | cmd_data[1];
                    device_state_.voltage = new_voltage;
                    LOG_INFO("Voltage set to %u mV", new_voltage);
                    
                    // Send response
                    if (response_size >= 2) {
                        response[0] = cmd_data[0];
                        response[1] = cmd_data[1];
                        *response_len = 2;
                    }
                    state_changed_ = true;
                    return ipc::ErrorCode::kNoError;
                }
                return ipc::ErrorCode::kCommandFailed;
            }
            case 0x11: {
                // Example command: Get device info
                if (response_size >= 4) {
                    response[0] = 0x01;  // Version
                    response[1] = 0x02;  // Model
                    response[2] = 0x00;  // Revision
                    response[3] = 0x00;  // Reserved
                    *response_len = 4;
                    LOG_INFO("Device info requested");
                    return ipc::ErrorCode::kNoError;
                }
                return ipc::ErrorCode::kBufferOverflow;
            }
            default:
                LOG_WARN("Unsupported command: 0x%02X", cmd_id);
                return ipc::ErrorCode::kCommandNotSupported;
        }
    }

    void update_state() {
        uint8_t state_data[sizeof(device_state_)];
        std::memcpy(state_data, &device_state_, sizeof(device_state_));
        slave_.send_state_update(0, state_data, sizeof(state_data));
        LOG_INFO("Sent unsolicited state update");
    }
};

/**
 * ============================================================================
 * USAGE EXAMPLE IN main.cpp
 * ============================================================================
 */

// In your main.cpp or similar:
/*

int main(void) {
    // Initialize HAL, clocks, etc.
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();  // Configure UART for RS485
    
    // Choose either MASTER or SLAVE configuration:
    
    #ifdef DEVICE_IS_MASTER
    {
        ipc::RS485Adapter rs485_adapter(&huart1);
        MasterApplication app(&rs485_adapter);
        
        if (!app.initialize()) {
            LOG_ERROR("Failed to initialize master application");
            return 1;
        }
        
        app.run_main_loop();
    }
    #else
    {
        ipc::RS485Adapter rs485_adapter(&huart1);
        SlaveApplication app(&rs485_adapter);
        
        if (!app.initialize(0x01)) {  // Slave address 0x01
            LOG_ERROR("Failed to initialize slave application");
            return 1;
        }
        
        app.run_main_loop();
    }
    #endif
    
    return 0;
}

*/
