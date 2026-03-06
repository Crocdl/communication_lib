#ifndef IPC_LOGGER_HPP
#define IPC_LOGGER_HPP

#include <cstdint>

namespace ipc {

enum class LogLevel : uint8_t { Error, Warn, Info, Debug };
using LogFn = void(*)(LogLevel, const char*, void*);

struct Logger {
    LogFn fn = nullptr;
    void* ctx = nullptr;
};

} // namespace ipc

#endif // IPC_LOGGER_HPP