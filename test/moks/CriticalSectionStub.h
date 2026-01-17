// CriticalSectionStub.h
#ifndef CRITICAL_SECTION_STUB_H
#define CRITICAL_SECTION_STUB_H

// Заглушки для тестирования на ПК
#include <mutex>
#include <atomic>

#ifdef __cplusplus
extern "C" {
#endif

// Структура для критической секции
typedef struct portMUX_TYPE {
    union {
        std::mutex* mtx;           // Для C++
        void* ptr;                 // Для C
    };
} portMUX_TYPE;

#define portMUX_INITIALIZER_UNLOCKED {NULL}

// Инициализация (опционально)
static inline void vPortCPUInitializeMutex(portMUX_TYPE* mux) {
    (void)mux;
}

// Обычные критические секции
static inline void portENTER_CRITICAL(portMUX_TYPE* mux) {
    if (mux) {
        if (!mux->mtx) {
            mux->mtx = new std::mutex();
        }
        mux->mtx->lock();
    }
}

static inline void portEXIT_CRITICAL(portMUX_TYPE* mux) {
    if (mux && mux->mtx) {
        mux->mtx->unlock();
    }
}

// Критические секции для прерываний
static inline void portENTER_CRITICAL_ISR(portMUX_TYPE* mux) {
    portENTER_CRITICAL(mux);
}

static inline void portEXIT_CRITICAL_ISR(portMUX_TYPE* mux) {
    portEXIT_CRITICAL(mux);
}

// Атомарные операции (заглушки)
static inline uint32_t portGET_INTERRUPT_MASK() {
    return 0;
}

static inline void portRESTORE_INTERRUPT_MASK(uint32_t mask) {
    (void)mask;
}

// Для совместимости с FreeRTOS
#define taskENTER_CRITICAL(mux) portENTER_CRITICAL(mux)
#define taskEXIT_CRITICAL(mux) portEXIT_CRITICAL(mux)
#define taskENTER_CRITICAL_ISR(mux) portENTER_CRITICAL_ISR(mux)
#define taskEXIT_CRITICAL_ISR(mux) portEXIT_CRITICAL_ISR(mux)
#define taskDISABLE_INTERRUPTS() do {} while(0)
#define taskENABLE_INTERRUPTS() do {} while(0)

#ifdef __cplusplus
}
#endif


#endif // CRITICAL_SECTION_STUB_H