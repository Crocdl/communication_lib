#include "../../../include/adapters/stm32_uart_adapter.hpp"
#include "../../../include/utils/logger.hpp"
#include <cstring>

namespace ipc {

STM32UARTAdapter* STM32UARTAdapter::instances_[STM32UARTAdapter::kMaxInstances] = {};

STM32UARTAdapter::STM32UARTAdapter(UART_HandleTypeDef* huart) noexcept
    : huart_(huart),
      config_(),
      rx_handler_(nullptr),
      rx_context_(nullptr),
      rx_byte_(0),
      rx_buffer_(),
      rx_head_(0),
      rx_tail_(0),
      tx_buffer_(),
      tx_busy_(false),
      initialized_(false),
      uart_errors_(0),
      rx_overruns_(0) {
    register_instance_();
}

STM32UARTAdapter::~STM32UARTAdapter() noexcept {
    unregister_instance_();
}

bool STM32UARTAdapter::init(const Config& config) noexcept {
    if (!huart_) {
        LOG_ERROR("STM32 UART handle is null");
        return false;
    }

    config_ = config;
    huart_->Init.BaudRate = config.baudrate;

#if defined(USE_HAL_UART_REGISTER_CALLBACKS) && (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    if (config_.register_hal_callbacks) {
        if (HAL_UART_RegisterCallback(huart_, HAL_UART_RX_COMPLETE_CB_ID,
                                      &STM32UARTAdapter::handle_rx_complete) != HAL_OK) {
            LOG_ERROR("STM32 UART RX callback registration failed");
            return false;
        }
        if (HAL_UART_RegisterCallback(huart_, HAL_UART_TX_COMPLETE_CB_ID,
                                      &STM32UARTAdapter::handle_tx_complete) != HAL_OK) {
            LOG_ERROR("STM32 UART TX callback registration failed");
            return false;
        }
        if (HAL_UART_RegisterCallback(huart_, HAL_UART_ERROR_CB_ID,
                                      &STM32UARTAdapter::handle_error) != HAL_OK) {
            LOG_ERROR("STM32 UART error callback registration failed");
            return false;
        }
    }
#endif

    initialized_ = start_rx_();
    if (!initialized_) {
        LOG_ERROR("STM32 UART RX start failed");
        return false;
    }

    LOG_INFO("STM32 UART adapter initialized at %lu baud", config_.baudrate);
    return true;
}

size_t STM32UARTAdapter::send(const byte* data, size_t len) noexcept {
    if (!initialized_ || !data || len == 0 || len > kTxBufferSize || tx_busy_) {
        return 0;
    }

    std::memcpy(tx_buffer_, data, len);
    tx_busy_ = true;

    if (HAL_UART_Transmit_IT(huart_, tx_buffer_, static_cast<uint16_t>(len)) != HAL_OK) {
        tx_busy_ = false;
        ++uart_errors_;
        return 0;
    }

    return len;
}

void STM32UARTAdapter::set_rx_handler(RxHandler handler, void* ctx) noexcept {
    rx_handler_ = handler;
    rx_context_ = ctx;
}

void STM32UARTAdapter::poll() noexcept {
    byte b = 0;
    while (pop_rx_byte_(&b)) {
        if (rx_handler_) {
            rx_handler_(b, rx_context_);
        }
    }
}

void STM32UARTAdapter::handle_rx_complete(UART_HandleTypeDef* huart) noexcept {
    STM32UARTAdapter* instance = find_instance_(huart);
    if (instance) {
        instance->on_rx_complete_();
    }
}

void STM32UARTAdapter::handle_tx_complete(UART_HandleTypeDef* huart) noexcept {
    STM32UARTAdapter* instance = find_instance_(huart);
    if (instance) {
        instance->on_tx_complete_();
    }
}

void STM32UARTAdapter::handle_error(UART_HandleTypeDef* huart) noexcept {
    STM32UARTAdapter* instance = find_instance_(huart);
    if (instance) {
        instance->on_error_();
    }
}

bool STM32UARTAdapter::register_instance_() noexcept {
    if (!huart_) {
        return false;
    }

    for (size_t i = 0; i < kMaxInstances; ++i) {
        if (instances_[i] == this) {
            return true;
        }
    }

    for (size_t i = 0; i < kMaxInstances; ++i) {
        if (!instances_[i]) {
            instances_[i] = this;
            return true;
        }
    }

    return false;
}

void STM32UARTAdapter::unregister_instance_() noexcept {
    for (size_t i = 0; i < kMaxInstances; ++i) {
        if (instances_[i] == this) {
            instances_[i] = nullptr;
            return;
        }
    }
}

bool STM32UARTAdapter::start_rx_() noexcept {
    if (!huart_) {
        return false;
    }

    return HAL_UART_Receive_IT(huart_, &rx_byte_, 1) == HAL_OK;
}

void STM32UARTAdapter::on_rx_complete_() noexcept {
    if (config_.buffer_rx_in_interrupt) {
        push_rx_byte_(rx_byte_);
    } else if (rx_handler_) {
        rx_handler_(rx_byte_, rx_context_);
    }

    if (!start_rx_()) {
        ++uart_errors_;
    }
}

void STM32UARTAdapter::on_tx_complete_() noexcept {
    tx_busy_ = false;
}

void STM32UARTAdapter::on_error_() noexcept {
    ++uart_errors_;
    uart_errors_ |= HAL_UART_GetError(huart_);
    tx_busy_ = false;
    HAL_UART_AbortReceive_IT(huart_);
    start_rx_();
}

void STM32UARTAdapter::push_rx_byte_(byte b) noexcept {
    const size_t next_head = (rx_head_ + 1) % kRxBufferSize;
    if (next_head == rx_tail_) {
        ++rx_overruns_;
        return;
    }

    rx_buffer_[rx_head_] = b;
    rx_head_ = next_head;
}

bool STM32UARTAdapter::pop_rx_byte_(byte* b) noexcept {
    if (!b || rx_tail_ == rx_head_) {
        return false;
    }

    *b = rx_buffer_[rx_tail_];
    rx_tail_ = (rx_tail_ + 1) % kRxBufferSize;
    return true;
}

STM32UARTAdapter* STM32UARTAdapter::find_instance_(UART_HandleTypeDef* huart) noexcept {
    for (size_t i = 0; i < kMaxInstances; ++i) {
        if (instances_[i] && instances_[i]->huart_ == huart) {
            return instances_[i];
        }
    }

    return nullptr;
}

} // namespace ipc
