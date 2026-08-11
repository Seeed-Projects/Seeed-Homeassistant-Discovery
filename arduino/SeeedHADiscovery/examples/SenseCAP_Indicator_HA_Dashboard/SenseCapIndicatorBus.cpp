#include "SenseCapIndicatorBus.h"

SenseCapIndicatorBus::SenseCapIndicatorBus(
    PCA9555& ioExpander, PCA95x5::Port::Port chipSelectPort,
    int8_t clockPin, int8_t dataPin)
    : ioExpander_(ioExpander),
      chipSelectPort_(chipSelectPort),
      clockPin_(clockPin),
      dataPin_(dataPin) {}

bool SenseCapIndicatorBus::begin(int32_t speed, int8_t dataMode) {
  _speed = speed;
  _dataMode = dataMode;

  pinMode(clockPin_, OUTPUT);
  pinMode(dataPin_, OUTPUT);
  digitalWrite(clockPin_, LOW);
  digitalWrite(dataPin_, LOW);
  setChipSelect(PCA95x5::Level::H);
  return ioExpander_.i2c_error() == 0;
}

void SenseCapIndicatorBus::beginWrite() {
  setChipSelect(PCA95x5::Level::L);
}

void SenseCapIndicatorBus::endWrite() {
  setChipSelect(PCA95x5::Level::H);
}

void SenseCapIndicatorBus::writeCommand(uint8_t command) {
  writeFrame(false, command);
}

void SenseCapIndicatorBus::writeCommand16(uint16_t command) {
  writeFrame(false, static_cast<uint8_t>(command >> 8));
  writeFrame(false, static_cast<uint8_t>(command));
}

void SenseCapIndicatorBus::writeCommandBytes(uint8_t* data,
                                             uint32_t length) {
  while (length-- > 0) {
    writeFrame(false, *data++);
  }
}

void SenseCapIndicatorBus::write(uint8_t data) {
  writeFrame(true, data);
}

void SenseCapIndicatorBus::write16(uint16_t data) {
  writeFrame(true, static_cast<uint8_t>(data >> 8));
  writeFrame(true, static_cast<uint8_t>(data));
}

void SenseCapIndicatorBus::writeRepeat(uint16_t pixel, uint32_t length) {
  while (length-- > 0) {
    write16(pixel);
  }
}

void SenseCapIndicatorBus::writeBytes(uint8_t* data, uint32_t length) {
  while (length-- > 0) {
    write(*data++);
  }
}

void SenseCapIndicatorBus::writePixels(uint16_t* data, uint32_t length) {
  while (length-- > 0) {
    write16(*data++);
  }
}

void SenseCapIndicatorBus::writeFrame(bool isData, uint8_t value) {
  digitalWrite(dataPin_, isData ? HIGH : LOW);
  digitalWrite(clockPin_, HIGH);
  digitalWrite(clockPin_, LOW);
  writeByte(value);
}

void SenseCapIndicatorBus::writeByte(uint8_t value) {
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    digitalWrite(dataPin_, (value & mask) ? HIGH : LOW);
    digitalWrite(clockPin_, HIGH);
    digitalWrite(clockPin_, LOW);
  }
}

void SenseCapIndicatorBus::setChipSelect(
    PCA95x5::Level::Level level) {
  ioExpander_.write(chipSelectPort_, level);
}
