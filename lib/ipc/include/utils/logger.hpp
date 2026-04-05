#ifndef IPC_LOGGER_HPP
#define IPC_LOGGER_HPP

#include <cstdint>
#include <string>
namespace ipc {

enum class LogLevel : uint8_t { Error, Warn, Info, Debug };
using LogFn = void(*)(LogLevel, const char*, void*);

struct Logger {
    LogFn fn = nullptr;
    void* ctx = nullptr;
};

} // namespace ipc
#include <cstdarg>
#include <cstdio>
void log_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    std::vprintf(fmt, args);
    std::printf("\n"); // перевод строки (как в LOG)

    va_end(args);
}
void log_info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    std::vprintf(fmt, args);
    std::printf("\n"); // перевод строки (как в LOG)

    va_end(args);
}
#define LOG_INFO(fmt, ...) log_info(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_error(fmt, ##__VA_ARGS__)
#endif // IPC_LOGGER_HPP