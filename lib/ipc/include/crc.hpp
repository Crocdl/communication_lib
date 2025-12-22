#ifndef IPC_CRC_HPP
#define IPC_CRC_HPP

#include <cstdint>
#include <cstddef>

namespace ipc {

template <typename T, T poly, T init, T xorout, bool reflect = false>
class CRC {
public:
    using value_type = T;
    
    CRC() noexcept : reg_(init) {}
    
    void reset() noexcept {
        reg_ = init;
    }
    
    void update(const uint8_t* data, size_t len) noexcept {
        for (size_t i = 0; i < len; ++i) {
            reg_ ^= static_cast<T>(data[i]) << (8 * (sizeof(T) - 1));
            
            for (int j = 0; j < 8; ++j) {
                if (reg_ & (static_cast<T>(1) << (8 * sizeof(T) - 1))) {
                    reg_ = (reg_ << 1) ^ poly;
                } else {
                    reg_ <<= 1;
                }
            }
        }
    }
    
    T finalize() const noexcept {
        return reg_ ^ xorout;
    }
    
private:
    T reg_;
};

using CRC16_CCITT  = CRC<uint16_t, 0x1021, 0xFFFF, 0x0000, false>;
using CRC16_MODBUS = CRC<uint16_t, 0x8005, 0xFFFF, 0x0000, false>;  // Упрощаем
using CRC32_IEEE   = CRC<uint32_t, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, false>;

} // namespace ipc

#endif // IPC_CRC_HPP