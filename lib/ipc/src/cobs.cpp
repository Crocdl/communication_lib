#include "../ipc/include/cobs.hpp"
namespace ipc {

/*
 * COBS encode
 * Возвращает количество записанных байт в out
 * 0 — ошибка (недостаточный размер буфера)
 */
size_t cobs_encode(const byte* in, size_t in_len,
                   byte* out, size_t out_size) noexcept
{
    if (!in || !out || out_size == 0) {
        return 0;
    }

    const byte* input = in;
    const byte* input_end = in + in_len;

    byte* output = out;
    byte* output_end = out + out_size;

    byte* code_ptr = output++;   // место под code
    byte code = 1;

    if (output > output_end) {
        return 0;
    }

    while (input < input_end) {
        if (*input == 0) {
            // записываем code
            *code_ptr = code;
            code = 1;
            code_ptr = output++;

            if (output > output_end) {
                return 0;
            }
        } else {
            if (output >= output_end) {
                return 0;
            }

            *output++ = *input;
            code++;

            if (code == 0xFF) {
                *code_ptr = code;
                code = 1;
                code_ptr = output++;

                if (output > output_end) {
                    return 0;
                }
            }
        }
        ++input;
    }

    // финальный code
    if (code_ptr >= output_end) {
        return 0;
    }
    *code_ptr = code;

    return static_cast<size_t>(output - out);
}

/*
 * COBS decode
 * Возвращает количество байт в out
 * 0 — ошибка формата
 */
size_t cobs_decode(const byte* in, size_t in_len,
                   byte* out, size_t out_size) noexcept
{
    if (!in || !out || in_len == 0) {
        return 0;
    }

    const byte* input = in;
    const byte* input_end = in + in_len;

    byte* output = out;
    byte* output_end = out + out_size;

    while (input < input_end) {
        byte code = *input++;
        if (code == 0) {
            // Нулевой байт внутри COBS-пакета — ошибка
            return 0;
        }

        // копируем (code - 1) байт
        for (byte i = 1; i < code; ++i) {
            if (input >= input_end || output >= output_end) {
                return 0;
            }
            *output++ = *input++;
        }

        // вставляем 0x00, если code < 0xFF и не конец
        if (code < 0xFF && input < input_end) {
            if (output >= output_end) {
                return 0;
            }
            *output++ = 0;
        }
    }

    return static_cast<size_t>(output - out);
}

} // namespace ipc
