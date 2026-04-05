#include "../include/adapters/can_adapter.hpp"
#include "../../../include/utils/logger.hpp"

// STM32 HAL headers
#ifdef STM32G4
#include "main.h"
#else

#endif

namespace ipc {

// Initialize static instance pointer
CANAdapter* CANAdapter::instance_ = nullptr;

CANAdapter::CANAdapter(CAN_HandleTypeDef* hcan) noexcept
    : hcan_(hcan),
      rx_handler_(nullptr),
      rx_context_(nullptr),
      rx_head_(0),
      rx_tail_(0),
      is_initialized_(false) {
    instance_ = this;
    
    // Clear RX buffer
    for (size_t i = 0; i < kMaxRxMessages; ++i) {
        rx_buffer_[i].can_id = 0;
        rx_buffer_[i].dlc = 0;
        rx_buffer_[i].is_fd = false;
        rx_buffer_[i].is_extended = false;
    }
}

CANAdapter::~CANAdapter() noexcept {
    if (is_initialized_) {
        HAL_CAN_Stop(hcan_);
    }
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

bool CANAdapter::init(const CanFdConfig& config) noexcept {
    if (!hcan_) {
        LOG_ERROR("CAN handle is null");
        return false;
    }

    // Configure CAN bitrate
    configure_bitrate_(config.nominal_bitrate, config.data_bitrate);

    // CAN FD configuration
    if (config.enable_fd) {
        hcan_->Init.FdMode = ENABLE;
        hcan_->Init.BaudRatePrescaler = 1;  // Will be set in configure_bitrate_
        hcan_->Init.TimingCharacteristics.NominalTimeSeg1 = 13;
        hcan_->Init.TimingCharacteristics.NominalTimeSeg2 = 2;
        hcan_->Init.TimingCharacteristics.NominalSJW = 1;
        hcan_->Init.TimingCharacteristics.DataTimeSeg1 = 5;
        hcan_->Init.TimingCharacteristics.DataTimeSeg2 = 1;
        hcan_->Init.TimingCharacteristics.DataSJW = 1;
    } else {
        hcan_->Init.FdMode = DISABLE;
    }

    // Basic CAN configuration
    hcan_->Init.Mode = CAN_MODE_NORMAL;
    hcan_->Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan_->Init.TimeTriggeredDebugMode = DISABLE;
    hcan_->Init.AutoBusOff = DISABLE;
    hcan_->Init.AutoWakeUp = DISABLE;
    hcan_->Init.AutoRetransmission = ENABLE;
    hcan_->Init.ReceiveMode = CAN_RX_FIFO_AUTO;
    hcan_->Init.TransmitGrp0Count = 3;
    hcan_->Init.TransmitGrp1Count = 2;
    hcan_->Init.TransmitGrp2Count = 2;

    // Initialize HAL CAN
    if (HAL_CAN_Init(hcan_) != HAL_OK) {
        LOG_ERROR("CAN initialization failed");
        return false;
    }

    // Setup filters for standard and extended IDs
    setup_filters_();

    // Register RX callbacks
    if (HAL_CAN_RegisterCallback(hcan_, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID,
                                  rx_fifo0_callback) != HAL_OK) {
        LOG_ERROR("Failed to register FIFO0 callback");
        return false;
    }

    if (HAL_CAN_RegisterCallback(hcan_, HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID,
                                  rx_fifo1_callback) != HAL_OK) {
        LOG_ERROR("Failed to register FIFO1 callback");
        return false;
    }

    if (HAL_CAN_RegisterCallback(hcan_, HAL_CAN_ERROR_CB_ID,
                                  error_callback) != HAL_OK) {
        LOG_ERROR("Failed to register error callback");
        return false;
    }

    // Enable CAN
    if (HAL_CAN_Start(hcan_) != HAL_OK) {
        LOG_ERROR("CAN start failed");
        return false;
    }

    // Enable interrupts
    if (HAL_CAN_ActivateNotification(hcan_,
                                     CAN_IT_RX_FIFO0_MSG_PENDING |
                                     CAN_IT_RX_FIFO1_MSG_PENDING |
                                     CAN_IT_ERROR) != HAL_OK) {
        LOG_ERROR("Failed to activate CAN interrupts");
        return false;
    }

    is_initialized_ = true;
    LOG_INFO("CAN adapter initialized successfully");
    return true;
}

size_t CANAdapter::send(const byte* data, size_t len) noexcept {
    if (!data || len == 0 || len > kMaxFrameLength) {
        return 0;
    }

    // Send as standard CAN frame with auto-assigned CAN ID (0x123 by default)
    if (send_can_message(0x123, data, len, false, false)) {
        return len;
    }
    return 0;
}

bool CANAdapter::send_can_message(uint32_t can_id, const byte* data, size_t len,
                                   bool is_extended, bool is_fd) noexcept {
    if (!is_initialized_ || !data || len == 0 || len > kMaxFrameLength) {
        return false;
    }

    CAN_TxHeaderTypeDef tx_header;
    uint8_t tx_data[kMaxFrameLength];

    // Copy data
    for (size_t i = 0; i < len; ++i) {
        tx_data[i] = data[i];
    }

    // Configure TX header
    tx_header.StdId = is_extended ? 0 : can_id;
    tx_header.ExtId = is_extended ? can_id : 0;
    tx_header.IDE = is_extended ? CAN_ID_EXT : CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = len;
    tx_header.FDF = is_fd ? ENABLE : DISABLE;
    tx_header.BRS = (is_fd && hcan_->Init.FdMode == ENABLE) ? ENABLE : DISABLE;
    tx_header.ESI = DISABLE;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t tx_mailbox;
    if (HAL_CAN_AddTxMessage(hcan_, &tx_header, tx_data, &tx_mailbox) != HAL_OK) {
        LOG_ERROR("CAN TX failed");
        return false;
    }

    return true;
}

void CANAdapter::set_rx_handler(RxHandler handler, void* ctx) noexcept {
    rx_handler_ = handler;
    rx_context_ = ctx;
}

void CANAdapter::poll() noexcept {
    if (!is_initialized_) {
        return;
    }

    RxMessage msg;
    while (pop_rx_message_(msg)) {
        if (rx_handler_) {
            // Call handler for each byte in the message
            for (size_t i = 0; i < msg.dlc; ++i) {
                rx_handler_(msg.data[i], rx_context_);
            }
        }
    }
}

void CANAdapter::setup_filters_() noexcept {
    CAN_FilterTypeDef filter_config;

    // Filter 0: Accept all standard IDs
    filter_config.FilterBank = 0;
    filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
    filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
    filter_config.FilterIdHigh = 0x0000;
    filter_config.FilterIdLow = 0x0000;
    filter_config.FilterMaskIdHigh = 0x0000;
    filter_config.FilterMaskIdLow = 0x0000;
    filter_config.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter_config.FilterActivation = CAN_FILTER_ENABLE;
    filter_config.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(hcan_, &filter_config) != HAL_OK) {
        LOG_ERROR("CAN filter configuration failed");
    }
}

void CANAdapter::configure_bitrate_(BaudRate nominal, BaudRate data) noexcept {
    // STM32G4 typical configuration for 170 MHz system clock
    // These values are approximate and may need adjustment based on actual clock
    
    // For 500 kbit/s nominal and CAN FD
    if (nominal == BaudRate::kBaud500k) {
        hcan_->Init.Prescaler = 17;
        hcan_->Init.TimeSeg1 = CAN_BS1_13TQ;
        hcan_->Init.TimeSeg2 = CAN_BS2_2TQ;
        hcan_->Init.SyncJumpWidth = CAN_SJW_1TQ;
    }
    // For 1 Mbit/s nominal
    else if (nominal == BaudRate::kBaud1M) {
        hcan_->Init.Prescaler = 8;
        hcan_->Init.TimeSeg1 = CAN_BS1_13TQ;
        hcan_->Init.TimeSeg2 = CAN_BS2_2TQ;
        hcan_->Init.SyncJumpWidth = CAN_SJW_1TQ;
    }
    // For other rates, provide safe defaults
    else {
        hcan_->Init.Prescaler = 17;
        hcan_->Init.TimeSeg1 = CAN_BS1_13TQ;
        hcan_->Init.TimeSeg2 = CAN_BS2_2TQ;
        hcan_->Init.SyncJumpWidth = CAN_SJW_1TQ;
    }
}

bool CANAdapter::is_rx_buffer_full_() const noexcept {
    return (rx_head_ + 1) % kMaxRxMessages == rx_tail_;
}

bool CANAdapter::is_rx_buffer_empty_() const noexcept {
    return rx_head_ == rx_tail_;
}

void CANAdapter::push_rx_message_(const RxMessage& msg) noexcept {
    if (is_rx_buffer_full_()) {
        return;
    }

    rx_buffer_[rx_head_] = msg;
    rx_head_ = (rx_head_ + 1) % kMaxRxMessages;
}

bool CANAdapter::pop_rx_message_(RxMessage& msg) noexcept {
    if (is_rx_buffer_empty_()) {
        return false;
    }

    msg = rx_buffer_[rx_tail_];
    rx_tail_ = (rx_tail_ + 1) % kMaxRxMessages;
    return true;
}

void CANAdapter::handle_rx_fifo0_() noexcept {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[kMaxFrameLength];

    while (HAL_CAN_GetRxMessage(hcan_, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
        RxMessage msg;
        msg.can_id = rx_header.IDE == CAN_ID_EXT ? rx_header.ExtId : rx_header.StdId;
        msg.dlc = rx_header.DLC;
        msg.is_fd = rx_header.FDF == ENABLE;
        msg.is_extended = rx_header.IDE == CAN_ID_EXT;

        for (size_t i = 0; i < msg.dlc; ++i) {
            msg.data[i] = rx_data[i];
        }

        push_rx_message_(msg);
        LOG_INFO("CAN message received: ID=0x%x, DLC=%u", msg.can_id, msg.dlc);
    }
}

void CANAdapter::handle_rx_fifo1_() noexcept {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[kMaxFrameLength];

    while (HAL_CAN_GetRxMessage(hcan_, CAN_RX_FIFO1, &rx_header, rx_data) == HAL_OK) {
        RxMessage msg;
        msg.can_id = rx_header.IDE == CAN_ID_EXT ? rx_header.ExtId : rx_header.StdId;
        msg.dlc = rx_header.DLC;
        msg.is_fd = rx_header.FDF == ENABLE;
        msg.is_extended = rx_header.IDE == CAN_ID_EXT;

        for (size_t i = 0; i < msg.dlc; ++i) {
            msg.data[i] = rx_data[i];
        }

        push_rx_message_(msg);
        LOG_INFO("CAN message received: ID=0x%x, DLC=%u", msg.can_id, msg.dlc);
    }
}

void CANAdapter::handle_error_() noexcept {
    // uint32_t errors = HAL_CAN_GetError(hcan_);
    // if (errors != HAL_CAN_ERROR_NONE) {
    //     LOG_ERROR("CAN error: 0x%x", errors);
    // }
}

// Static callback implementations
void CANAdapter::rx_fifo0_callback(CAN_HandleTypeDef* hcan) {
    if (instance_) {
        instance_->handle_rx_fifo0_();
    }
}

void CANAdapter::rx_fifo1_callback(CAN_HandleTypeDef* hcan) {
    if (instance_) {
        instance_->handle_rx_fifo1_();
    }
}

void CANAdapter::error_callback(CAN_HandleTypeDef* hcan) {
    if (instance_) {
        instance_->handle_error_();
    }
}

} // namespace ipc
