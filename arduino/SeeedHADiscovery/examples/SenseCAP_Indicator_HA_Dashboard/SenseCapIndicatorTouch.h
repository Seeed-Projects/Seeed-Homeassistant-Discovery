#pragma once

#include <Arduino.h>
#include <PCA95x5.h>
#include <Wire.h>

class SenseCapIndicatorTouch {
 public:
  SenseCapIndicatorTouch(TwoWire& wire, PCA9555& ioExpander,
                         PCA95x5::Port::Port resetPort,
                         int8_t sdaPin, int8_t sclPin, uint32_t frequency,
                         uint16_t width, uint16_t height);

  bool begin(uint8_t displayRotation);
  bool read(uint16_t& x, uint16_t& y);

 private:
  // Reads a consecutive register block from the FT5x06 controller.
  // 从 FT5x06 控制器读取一段连续寄存器。
  bool readRegisters(uint8_t startRegister, uint8_t* data, size_t length);

  // Writes one FT5x06 configuration register.
  // 写入一个 FT5x06 配置寄存器。
  bool writeRegister(uint8_t registerAddress, uint8_t value);

  // Reports repeated coordinate read failures at a limited rate.
  // 以受限频率报告连续的坐标读取失败。
  void logReadError();

  // Reinitializes the I2C bus and resets the touch controller.
  // 重新初始化 I2C 总线并复位触摸控制器。
  void recoverController(const char* reason);

  // Applies the FT5x06 operating thresholds used by the reference driver.
  // 应用参考驱动使用的 FT5x06 工作阈值。
  bool configureController();

  // Converts raw controller coordinates into the active display orientation.
  // 将触摸控制器原始坐标转换为当前屏幕方向坐标。
  void transform(uint16_t rawX, uint16_t rawY, uint16_t& x,
                 uint16_t& y) const;

  static constexpr uint8_t kTouchAddress = 0x48;
  static constexpr uint8_t kTouchPointsRegister = 0x02;
  static constexpr uint8_t kFirstPointRegister = 0x03;
  static constexpr size_t kFirstPointDataLength = 4;

  TwoWire& wire_;
  PCA9555& ioExpander_;
  PCA95x5::Port::Port resetPort_;
  int8_t sdaPin_;
  int8_t sclPin_;
  uint32_t frequency_;
  uint16_t width_;
  uint16_t height_;
  uint8_t rotation_ = 0;
  bool initialized_ = false;
  bool lastPressed_ = false;
  uint8_t consecutiveReadFailures_ = 0;
  uint32_t lastReadErrorLogAt_ = 0;
  uint32_t lastRecoveryAt_ = 0;
};
