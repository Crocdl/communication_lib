#ifndef IPC_CODEC_HPP
#define IPC_CODEC_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include "./cobs.hpp"

namespace ipc {

using byte = uint8_t;

enum class DecodeError {
    None,
    MalformedCOBS,
    CRCMismatch,
    PayloadTooLarge
};

template <typename CRC_t>
size_t encode_frame(const byte* payload, size_t payload_len,
                    byte* out, size_t out_size) noexcept
{
    // === РЕАЛИЗАЦИЯ ===
    if (!payload || !out || out_size == 0) {
        return 0;
    }

    // 1. Рассчитываем CRC
    CRC_t crc;
    crc.reset();
    crc.update(payload, payload_len);
    auto crc_value = crc.finalize();

    // 2. Готовим буфер для COBS: payload + CRC
    const size_t crc_size = sizeof(crc_value);
    const size_t raw_len = payload_len + crc_size;
    
    // Временный буфер на стеке (можно сделать динамически для больших данных)
    byte raw_buf[256];
    if (raw_len > sizeof(raw_buf)) {
        return 0; // Слишком большой пакет
    }
    
    std::memcpy(raw_buf, payload, payload_len);
    std::memcpy(raw_buf + payload_len, &crc_value, crc_size);

    // 3. Кодируем COBS
    size_t enc_len = cobs_encode(raw_buf, raw_len, out, out_size);
    if (enc_len == 0) {
        return 0;
    }

    return enc_len;
}

template <typename CRC_t>
bool decode_frame(const byte* enc, size_t enc_len,
                  byte* out_payload, size_t out_payload_size,
                  size_t* payload_len,
                  DecodeError* err = nullptr) noexcept
{
    // === РЕАЛИЗАЦИЯ ===
    if (err) *err = DecodeError::None;

    if (!enc || !out_payload || enc_len == 0) {
        if (err) *err = DecodeError::MalformedCOBS;
        return false;
    }

    // 1. Декодируем COBS
    byte decoded[256];  // временный буфер
    size_t decoded_len = cobs_decode(enc, enc_len, decoded, sizeof(decoded));
    if (decoded_len == 0) {
        if (err) *err = DecodeError::MalformedCOBS;
        return false;
    }

    // 2. Извлекаем CRC
    const size_t crc_size = sizeof(decltype(std::declval<CRC_t>().finalize()));
    if (decoded_len < crc_size) {
        if (err) *err = DecodeError::MalformedCOBS;
        return false;
    }

    size_t actual_payload_len = decoded_len - crc_size;
    if (actual_payload_len > out_payload_size) {
        if (err) *err = DecodeError::PayloadTooLarge;
        return false;
    }

    // 3. Копируем payload
    std::memcpy(out_payload, decoded, actual_payload_len);

    // 4. Проверяем CRC
    typename CRC_t::value_type received_crc;
    std::memcpy(&received_crc, decoded + actual_payload_len, crc_size);

    CRC_t crc;
    crc.reset();
    crc.update(out_payload, actual_payload_len);
    auto calculated_crc = crc.finalize();

    if (received_crc != calculated_crc) {
        if (err) *err = DecodeError::CRCMismatch;
        return false;
    }

    if (payload_len) {
        *payload_len = actual_payload_len;
    }

    return true;
}

} // namespace ipc

#endif // IPC_CODEC_HPP