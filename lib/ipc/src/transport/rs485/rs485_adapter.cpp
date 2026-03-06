#include "../include/adapters/rs485_adapter.hpp"
#include "../include/logger.hpp"

namespace ipc {

// Initialize static instance pointer
RS485Adapter* RS485Adapter::instance_ = nullptr;

RS485Adapter::RS485Adapter(UART_HandleTypeDef* huart) noexcept
    : huart_(huart), rx_handler_(nullptr), rx_context_(nullptr),
      rx_idx_(0), is_initialized_(false), de_state_(false) {
    instance_ = this;
}

bool RS485Adapter::init(const Config& config) noexcept {
    if (!huart_) {
        LOG_ERROR("UART handle is null");
        return false;
    }

    config_ = config;

    // Configure UART baudrate
    huart_->Init.BaudRate = config.baudrate;

    // If using automatic DE control, configure GPIO
    if (config.de_mode == DEMode::kAutomatic) {
        // DE/RE GPIO should be configured in CubeMX
        // Here we just initialize the adapter
        LOG_INFO("RS485 Automatic DE mode configured");
    } else {
        LOG_INFO("RS485 Manual DE mode configured");
    }

    // Register UART callbacks
    if (HAL_UART_RegisterCallback(huart_, HAL_UART_RX_COMPLETE_CB_ID,
                                   rx_callback_) != HAL_OK) {
        LOG_ERROR("Failed to register RX callback");
        return false;
    }

    if (HAL_UART_RegisterCallback(huart_, HAL_UART_TX_COMPLETE_CB_ID,
                                   tx_complete_callback_) != HAL_OK) {
        LOG_ERROR("Failed to register TX callback");
        return false;
    }

    if (HAL_UART_RegisterCallback(huart_, HAL_UART_ERROR_CB_ID,
                                   error_callback_) != HAL_OK) {
        LOG_ERROR("Failed to register error callback");
        return false;
    }

    // Enable receive interrupt (DMA or IT mode)
    // For single byte reception to detect frame markers
    if (HAL_UART_Receive_IT(huart_, rx_buffer_, 1) != HAL_OK) {
        LOG_ERROR("Failed to enable RX interrupt");
        return false;
    }

    is_initialized_ = true;
    deassert_de_();  // Start with DE low (receive mode)
    
    LOG_INFO("RS485 adapter initialized at %lu baud", config.baudrate);
    return true;
}

size_t RS485Adapter::send(const byte* data, size_t len) noexcept {
    if (!is_initialized_ || !data || len == 0) {
        return 0;
    }

    // Assert DE line for transmission
    assert_de_();

    // Wait a bit for the line to stabilize
    // In real implementation, use a timer for de_assert_delay
    // For now, minimal delay
    
    // Transmit data
    if (HAL_UART_Transmit_IT(huart_, const_cast<byte*>(data), len) != HAL_OK) {
        LOG_ERROR("UART TX failed");
        deassert_de_();  // Release DE line on error
        return 0;
    }

    return len;
}

void RS485Adapter::set_rx_handler(RxHandler handler, void* ctx) noexcept {
    rx_handler_ = handler;
    rx_context_ = ctx;
}

void RS485Adapter::poll() noexcept {
    // In interrupt-driven mode, nothing to do here
    // The callbacks handle data processing
}

void RS485Adapter::set_de_state(bool active) noexcept {
    if (config_.de_mode == DEMode::kManual) {
        if (active) {
            assert_de_();
        } else {
            deassert_de_();
        }
    }
}

void RS485Adapter::assert_de_() noexcept {
    // Set DE and optionally RE to high for transmission
    // Assuming GPIO_PIN_DE_RE is defined in main.h
    #ifdef GPIO_PIN_DE_RE
    HAL_GPIO_WritePin(PORT_DE_RE, GPIO_PIN_DE_RE, GPIO_PIN_SET);
    #endif
    de_state_ = true;
    LOG_DEBUG("DE asserted");
}

void RS485Adapter::deassert_de_() noexcept {
    // Set DE and RE to low for reception
    #ifdef GPIO_PIN_DE_RE
    HAL_GPIO_WritePin(PORT_DE_RE, GPIO_PIN_DE_RE, GPIO_PIN_RESET);
    #endif
    de_state_ = false;
    LOG_DEBUG("DE deasserted");
}

void RS485Adapter::enable_rx_() noexcept {
    if (HAL_UART_Receive_IT(huart_, rx_buffer_, 1) != HAL_OK) {
        LOG_ERROR("Failed to enable RX");
    }
}

void RS485Adapter::disable_rx_() noexcept {
    HAL_UART_AbortReceive_IT(huart_);
}

void RS485Adapter::rx_callback_(UART_HandleTypeDef* huart) {
    if (instance_) {
        // Call handler for received byte
        if (instance_->rx_handler_) {
            instance_->rx_handler_(instance_->rx_buffer_[0], instance_->rx_context_);
        }
        
        // Re-enable RX for next byte
        if (HAL_UART_Receive_IT(huart, instance_->rx_buffer_, 1) != HAL_OK) {
            LOG_ERROR("RX re-enable failed");
        }
    }
}

void RS485Adapter::tx_complete_callback_(UART_HandleTypeDef* huart) {
    if (instance_) {
        // Deassert DE line after transmission
        // In production, implement proper timing with delay
        instance_->deassert_de_();
        
        // Re-enable RX
        if (HAL_UART_Receive_IT(huart, instance_->rx_buffer_, 1) != HAL_OK) {
            LOG_ERROR("Failed to re-enable RX after TX");
        }
        
        LOG_DEBUG("TX complete, DE deasserted");
    }
}

void RS485Adapter::error_callback_(UART_HandleTypeDef* huart) {
    if (instance_) {
        uint32_t error = HAL_UART_GetError(huart);
        LOG_ERROR("UART error: 0x%x", error);
        
        // Attempt recovery
        HAL_UART_DeInit(huart);
        // Re-initialize with previous config would go here
        instance_->enable_rx_();
    }
}

} // namespace ipc
