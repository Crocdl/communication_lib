#pragma once
#include "IpcTypes.h"

namespace ipc {

// Политика повторов
template <size_t MaxRetries>
struct RetryPolicy {
    static constexpr size_t max_retries = MaxRetries;
};

// Политика таймаута
template <time_ms_t TimeoutMs>
struct TimeoutPolicy {
    static constexpr time_ms_t timeout_ms = TimeoutMs;
};

} // namespace ipc
