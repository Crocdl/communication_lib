#include <cstdint>
#include <cstddef>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1,
    HAL_BUSY = 2
} HAL_StatusTypeDef;
struct UART_InitTypeDef {
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
    uint32_t Mode;
    uint32_t HwFlowCtl;
    uint32_t OverSampling;
};

struct UART_HandleTypeDef;
typedef void (*pUART_CallbackTypeDef)(UART_HandleTypeDef*);
struct UART_HandleTypeDef {
    void* Instance;  
    
    UART_InitTypeDef Init; 

    uint8_t* tx_buffer;
    size_t tx_size;

    uint8_t* rx_buffer;
    size_t rx_size;

    bool tx_in_progress;
    bool rx_in_progress;

    pUART_CallbackTypeDef tx_cb;
    pUART_CallbackTypeDef rx_cb;
    pUART_CallbackTypeDef err_cb;

    uint32_t last_error;
};


extern "C" {

// Глобальные callback'и (имитируем HAL weak symbols)
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart);
void HAL_UART_AbortReceive_IT(UART_HandleTypeDef* huart);

HAL_StatusTypeDef HAL_UART_Transmit(
    UART_HandleTypeDef* huart,
    uint8_t* data,
    uint16_t size,
    uint32_t timeout)
{
    huart->tx_buffer = data;
    huart->tx_size = size;
    huart->tx_in_progress = false;

    // сразу считаем, что отправка завершилась
    HAL_UART_TxCpltCallback(huart);

    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(
    UART_HandleTypeDef* huart,
    uint8_t* data,
    uint16_t size)
{
    huart->tx_buffer = data;
    huart->tx_size = size;
    huart->tx_in_progress = true;

    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(
    UART_HandleTypeDef* huart,
    uint8_t* data,
    uint16_t size)
{
    huart->rx_buffer = data;
    huart->rx_size = size;
    huart->rx_in_progress = true;

    return HAL_OK;
}

} // extern "C"
typedef enum {
    HAL_UART_TX_COMPLETE_CB_ID = 0,
    HAL_UART_RX_COMPLETE_CB_ID = 1,
    HAL_UART_ERROR_CB_ID       = 2
} HAL_UART_CallbackIDTypeDef;



// --------------------------------------------------
// Register callback
// --------------------------------------------------

inline HAL_StatusTypeDef HAL_UART_RegisterCallback(
    UART_HandleTypeDef* huart,
    HAL_UART_CallbackIDTypeDef id,
    pUART_CallbackTypeDef cb)
{
    switch (id) {
        case HAL_UART_TX_COMPLETE_CB_ID:
            huart->tx_cb = cb;
            break;
        case HAL_UART_RX_COMPLETE_CB_ID:
            huart->rx_cb = cb;
            break;
        case HAL_UART_ERROR_CB_ID:
            huart->err_cb = cb;
            break;
        default:
            return HAL_ERROR;
    }
    return HAL_OK;
}

// --------------------------------------------------
// Get error
// --------------------------------------------------

inline uint32_t HAL_UART_GetError(UART_HandleTypeDef* huart)
{
    return huart->last_error;
}

// --------------------------------------------------
// DeInit
// --------------------------------------------------

inline HAL_StatusTypeDef HAL_UART_DeInit(UART_HandleTypeDef* huart)
{
    huart->tx_in_progress = false;
    huart->rx_in_progress = false;
    huart->last_error = 0;
    return HAL_OK;
}

// --------------------------------------------------
// Abort RX
// --------------------------------------------------

inline void HAL_UART_AbortReceive_IT(UART_HandleTypeDef* huart)
{
    huart->rx_in_progress = false;
}
