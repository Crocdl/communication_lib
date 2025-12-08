#include "com.h"
byte Communicator::crc8_bytes(byte *buffer, byte size)
{
    byte crc = 0;
    for (byte i = 0; i < size; i++)
    {
        byte data = buffer[i];
        for (int j = 8; j > 0; j--)
        {
            crc = ((crc ^ data) & 1) ? (crc >> 1) ^ 0x8C : (crc >> 1);
            data >>= 1;
        }
    }
    return crc;
}
int Communicator::bytetoint(uint8_t *byteArray, uint32_t start)
{
    return ((long)byteArray[start] << 24) |
           ((long)byteArray[start + 1] << 16) |
           ((long)byteArray[start + 2] << 8) |
           byteArray[start + 3];
}
void Communicator::inttobyte(int32_t in, uint8_t *buffer, size_t index)
{
    buffer[index] = (in >> 24) & 0xFF;
    buffer[index + 1] = (in >> 16) & 0xFF;
    buffer[index + 2] = (in >> 8) & 0xFF;
    buffer[index + 3] = in & 0xFF;
    return;
}
int Communicator::read(uint8_t *inbuffer, int bufferSize)
{
    int index = 0;
    uint32_t lastByteTime = millis();
    if (!MySerial.available())
        return 0;
    while (true)
    {
        // Проверяем наличие данных
        if (MySerial.available())
        {
            inbuffer[index++] = MySerial.read();
            lastByteTime = millis();

            // Проверяем переполнение буфера
            if (index >= bufferSize)
            {
                return -1; // Ошибка: слишком длинный пакет
            }
        }

        // Если данных нет и таймаут между байтами истек
        if (millis() - lastByteTime > 2)
        {
            break;
        }
    }

    // Проверяем минимальный размер пакета
    if (index < 4)
    {
        return -2; // Ошибка: слишком короткий пакет
    }
    // Проверяем CRC
    Serial.print(' ');
    for (int i = 0; i < index; i++)
    {
        Serial.print(inbuffer[i], HEX);
        Serial.print(' ');
    }
    Serial.println(' ');
    byte receivedCRC = inbuffer[index - 1];
    byte calculatedCRC = crc8_bytes(inbuffer, index - 1);
    if (receivedCRC != calculatedCRC)
    {
        return -3; // Ошибка: некорректный CRC
    }

    return index; // Возвращаем длину пакета
}
void Communicator::write(uint8_t *writebuffer, size_t size)
{
    MySerial.write(writebuffer, size);
    MySerial.flush();
    Serial.println(writebuffer[3], HEX);
    Serial.print(' ');
    for (int i = 0; i < 4; i++)
    {
        Serial.print(writebuffer[i], HEX);
        Serial.print(' ');
    }
    Serial.println(' ');
}