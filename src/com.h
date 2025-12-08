#include <Arduino.h>
#include <HardwareSerial.h>
#define ASCII_CONVERT '0'
#define BUFFER_SIZE 32
class Communicator
{
private:
  Stream &MySerial;
public:
  byte crc8_bytes(byte *buffer, byte size);
  int bytetoint(uint8_t *byteArray, uint32_t start);
  void inttobyte(int32_t in, uint8_t *buffer, size_t index);
  Communicator(Stream &s) : MySerial(s)
  {
  }
  int read(uint8_t *inbuffer, int bufferSize);
  void write(uint8_t *writebuffer, size_t size);
};