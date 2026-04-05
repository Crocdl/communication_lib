#ifndef MOCK_CAN_ADAPTER_HPP
#define MOCK_CAN_ADAPTER_HPP

#include <cstdint>
#include <cstddef>
#include <queue>
#include <functional>

// ===================== Константы HAL =====================
#define ENABLE  1
#define DISABLE 0
#define HAL_OK 0

#define CAN_MODE_NORMAL 0
#define CAN_ID_STD 0
#define CAN_ID_EXT 1
#define CAN_RTR_DATA 0
#define CAN_RX_FIFO_AUTO 0
#define CAN_IT_RX_FIFO0_MSG_PENDING 0
#define CAN_IT_RX_FIFO1_MSG_PENDING 0
#define CAN_IT_ERROR 0
#define CAN_SJW_1TQ 1

#define CAN_FILTERMODE_IDMASK 0
#define CAN_FILTERSCALE_32BIT 0
#define CAN_FILTER_FIFO0 0
#define CAN_FILTER_ENABLE 1

#define CAN_BS1_13TQ 13
#define CAN_BS2_2TQ 2

#define CAN_RX_FIFO1 1
#define CAN_RX_FIFO0 0
// ===================== Callback ID =====================
typedef enum {
    HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID,
    HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID,
    HAL_CAN_ERROR_CB_ID
} HAL_CAN_CallbackIDTypeDef;

// ===================== Структуры =====================
struct CAN_TimingTypeDef {
    uint32_t NominalTimeSeg1;
    uint32_t NominalTimeSeg2;
    uint32_t NominalSJW;
    uint32_t DataTimeSeg1;
    uint32_t DataTimeSeg2;
    uint32_t DataSJW;
};

struct CAN_InitTypeDef {
    bool FdMode;
    uint32_t BaudRatePrescaler;
    CAN_TimingTypeDef TimingCharacteristics;
    uint32_t Mode;
    uint32_t SyncJumpWidth;
    bool TimeTriggeredDebugMode;
    bool AutoBusOff;
    uint8_t ReceiveMode;
    bool AutoWakeUp;
    bool AutoRetransmission;
    uint8_t TransmitGrp0Count;
    uint8_t TransmitGrp1Count;
    uint8_t TransmitGrp2Count;
    uint32_t Prescaler;
    uint32_t TimeSeg1;
    uint32_t TimeSeg2;
};

struct CAN_HandleTypeDef {
    void* Instance;
    CAN_InitTypeDef Init;

    typedef void (*pCAN_CallbackTypeDef)(CAN_HandleTypeDef*);
    pCAN_CallbackTypeDef rx_fifo0_cb;
    pCAN_CallbackTypeDef rx_fifo1_cb;
    pCAN_CallbackTypeDef err_cb;
};

struct CAN_TxHeaderTypeDef {
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    bool FDF;
    bool BRS;
    bool ESI;
    bool TransmitGlobalTime;
    uint32_t StdId;
    uint32_t ExtId;
};
struct CAN_RxHeaderTypeDef{
        uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    bool FDF;
    bool BRS;
    bool ESI;
    bool TransmitGlobalTime;
    uint32_t StdId;
    uint32_t ExtId;
};

struct CAN_FilterTypeDef {
    uint32_t FilterId;
    uint32_t FilterMask;
    uint32_t FilterBank;
    uint32_t FilterMode;
    uint32_t FilterScale;
    uint32_t FilterIdHigh;
    uint32_t FilterIdLow;
    uint32_t FilterMaskIdHigh;
    uint32_t FilterMaskIdLow;
    uint32_t FilterFIFOAssignment;
    uint32_t FilterActivation;
    uint32_t SlaveStartFilterBank;
};

// ===================== HAL-функции =====================
inline int HAL_CAN_Init(CAN_HandleTypeDef* hcan) { return HAL_OK; }
inline int HAL_CAN_Stop(CAN_HandleTypeDef* hcan) { return HAL_OK; }
inline int HAL_CAN_Start(CAN_HandleTypeDef* hcan) { return HAL_OK; }
inline int HAL_CAN_ActivateNotification(CAN_HandleTypeDef* hcan, uint32_t it) { return HAL_OK; }
inline int HAL_CAN_ConfigFilter(CAN_HandleTypeDef* hcan, CAN_FilterTypeDef* filter) { return HAL_OK; }
inline int HAL_CAN_AddTxMessage(CAN_HandleTypeDef* hcan, CAN_TxHeaderTypeDef* header, uint8_t* data, uint32_t* mailbox) { return HAL_OK; }
inline int HAL_CAN_GetRxMessage( CAN_HandleTypeDef* hcan, uint32_t RxFifo, CAN_RxHeaderTypeDef* pHeader, uint8_t* pData)                
{return HAL_OK;}
inline int HAL_CAN_RegisterCallback(CAN_HandleTypeDef* hcan, HAL_CAN_CallbackIDTypeDef id, CAN_HandleTypeDef::pCAN_CallbackTypeDef cb) {
    switch(id) {
        case HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID: hcan->rx_fifo0_cb = cb; break;
        case HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID: hcan->rx_fifo1_cb = cb; break;
        case HAL_CAN_ERROR_CB_ID: hcan->err_cb = cb; break;
        default: return 1;
    }
    return HAL_OK;
}

#endif // MOCK_CAN_ADAPTER_HPP