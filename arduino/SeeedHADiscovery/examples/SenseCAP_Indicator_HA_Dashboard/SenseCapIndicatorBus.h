#pragma once

#include <Arduino_DataBus.h>
#include <PCA95x5.h>

class SenseCapIndicatorBus : public Arduino_DataBus {
 public:
  SenseCapIndicatorBus(PCA9555& ioExpander, PCA95x5::Port::Port chipSelectPort,
                       int8_t clockPin, int8_t dataPin);

  bool begin(int32_t speed = SPI_DEFAULT_FREQ,
             int8_t dataMode = GFX_NOT_DEFINED) override;
  void beginWrite() override;
  void endWrite() override;
  void writeCommand(uint8_t command) override;
  void writeCommand16(uint16_t command) override;
  void writeCommandBytes(uint8_t* data, uint32_t length) override;
  void write(uint8_t data) override;
  void write16(uint16_t data) override;
  void writeRepeat(uint16_t pixel, uint32_t length) override;
  void writeBytes(uint8_t* data, uint32_t length) override;
  void writePixels(uint16_t* data, uint32_t length) override;

 private:
  // Sends one 9-bit frame: the first bit selects command or data.
  // 发送一个 9 位帧：第一位用于区分命令和数据。
  void writeFrame(bool isData, uint8_t value);
  void writeByte(uint8_t value);
  void setChipSelect(PCA95x5::Level::Level level);

  PCA9555& ioExpander_;
  PCA95x5::Port::Port chipSelectPort_;
  int8_t clockPin_;
  int8_t dataPin_;
};
