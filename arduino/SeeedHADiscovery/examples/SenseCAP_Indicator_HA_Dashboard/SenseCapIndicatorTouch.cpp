#include "SenseCapIndicatorTouch.h"

namespace {

struct RegisterSetting {
  uint8_t address;
  uint8_t value;
};

constexpr RegisterSetting kInitializationSettings[] = {
    {0x80, 70},
    {0x81, 60},
    {0x82, 16},
    {0x83, 60},
    {0x84, 10},
    {0x85, 20},
    {0x87, 2},
    {0x88, 12},
    {0x89, 40},
};

constexpr uint32_t kReadErrorLogIntervalMs = 1000;
constexpr uint32_t kRecoveryIntervalMs = 1000;
constexpr uint16_t kI2cTimeoutMs = 20;
constexpr uint8_t kReadFailuresBeforeRecovery = 3;
constexpr uint8_t kTouchEventLiftUp = 1;

}  // namespace

SenseCapIndicatorTouch::SenseCapIndicatorTouch(
    TwoWire& wire, PCA9555& ioExpander,
    PCA95x5::Port::Port resetPort,
    int8_t sdaPin, int8_t sclPin, uint32_t frequency,
    uint16_t width, uint16_t height)
    : wire_(wire),
      ioExpander_(ioExpander),
      resetPort_(resetPort),
      sdaPin_(sdaPin),
      sclPin_(sclPin),
      frequency_(frequency),
      width_(width),
      height_(height) {}

bool SenseCapIndicatorTouch::begin(uint8_t displayRotation) {
  rotation_ = displayRotation % 4;
  wire_.setTimeOut(kI2cTimeoutMs);
  wire_.beginTransmission(kTouchAddress);
  initialized_ = wire_.endTransmission() == 0;
  if (!initialized_) {
    return false;
  }

  if (!configureController()) {
    Serial.println("Touch controller configuration warning");
  }
  consecutiveReadFailures_ = 0;
  lastPressed_ = false;
  return initialized_;
}

bool SenseCapIndicatorTouch::read(uint16_t& x, uint16_t& y) {
  if (!initialized_) {
    return false;
  }

  uint8_t touchPoints = 0;
  if (!readRegisters(kTouchPointsRegister, &touchPoints, 1)) {
    logReadError();
    return false;
  }
  consecutiveReadFailures_ = 0;
  if ((touchPoints & 0x0F) == 0) {
    if (lastPressed_) {
      Serial.println("Touch released");
    }
    lastPressed_ = false;
    return false;
  }

  uint8_t pointData[kFirstPointDataLength] = {};
  if (!readRegisters(kFirstPointRegister, pointData, sizeof(pointData))) {
    logReadError();
    return false;
  }
  consecutiveReadFailures_ = 0;

  const uint8_t touchEvent = pointData[0] >> 6;
  if (touchEvent == kTouchEventLiftUp) {
    if (lastPressed_) {
      Serial.println("Touch released");
    }
    lastPressed_ = false;
    return false;
  }

  const uint16_t rawX = ((pointData[0] & 0x0F) << 8) | pointData[1];
  const uint16_t rawY = ((pointData[2] & 0x0F) << 8) | pointData[3];
  transform(rawX, rawY, x, y);
  x = constrain(x, 0, width_ - 1);
  y = constrain(y, 0, height_ - 1);
  if (!lastPressed_) {
    Serial.printf("Touch pressed: raw=(%u, %u), mapped=(%u, %u)\n",
                  rawX, rawY, x, y);
  }
  lastPressed_ = true;
  return true;
}

bool SenseCapIndicatorTouch::readRegisters(
    uint8_t startRegister, uint8_t* data, size_t length) {
  wire_.beginTransmission(kTouchAddress);
  wire_.write(startRegister);
  if (wire_.endTransmission() != 0) {
    return false;
  }

  const size_t received = wire_.requestFrom(kTouchAddress, length);
  if (received != length) {
    while (wire_.available()) {
      wire_.read();
    }
    return false;
  }
  return wire_.readBytes(data, length) == length;
}

bool SenseCapIndicatorTouch::writeRegister(
    uint8_t registerAddress, uint8_t value) {
  wire_.beginTransmission(kTouchAddress);
  wire_.write(registerAddress);
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

void SenseCapIndicatorTouch::logReadError() {
  lastPressed_ = false;
  ++consecutiveReadFailures_;
  const uint32_t now = millis();
  if (now - lastReadErrorLogAt_ >= kReadErrorLogIntervalMs) {
    Serial.println("Touch coordinate read failed");
    lastReadErrorLogAt_ = now;
  }
  if (consecutiveReadFailures_ >= kReadFailuresBeforeRecovery) {
    recoverController("repeated I2C read failures");
  }
}

void SenseCapIndicatorTouch::recoverController(const char* reason) {
  const uint32_t now = millis();
  if (now - lastRecoveryAt_ < kRecoveryIntervalMs) {
    return;
  }
  lastRecoveryAt_ = now;
  initialized_ = false;
  lastPressed_ = false;

  Serial.printf("Touch recovery started: %s\n", reason);
  wire_.end();
  delay(2);
  wire_.begin(sdaPin_, sclPin_, frequency_);
  wire_.setTimeOut(kI2cTimeoutMs);
  ioExpander_.attach(wire_);
  ioExpander_.write(resetPort_, PCA95x5::Level::L);
  delay(5);
  ioExpander_.write(resetPort_, PCA95x5::Level::H);
  delay(20);

  if (begin(rotation_)) {
    Serial.println("Touch controller recovered");
  } else {
    Serial.println("Touch controller recovery failed");
  }
}

bool SenseCapIndicatorTouch::configureController() {
  bool configurationReady = true;
  for (const RegisterSetting& setting : kInitializationSettings) {
    configurationReady &= writeRegister(setting.address, setting.value);
  }
  return configurationReady;
}

void SenseCapIndicatorTouch::transform(
    uint16_t rawX, uint16_t rawY, uint16_t& x, uint16_t& y) const {
  switch (rotation_) {
    case 1:
      x = height_ - 1 - rawY;
      y = rawX;
      break;
    case 2:
      x = rawX;
      y = rawY;
      break;
    case 3:
      x = rawY;
      y = width_ - 1 - rawX;
      break;
    default:
      x = width_ - 1 - rawX;
      y = height_ - 1 - rawY;
      break;
  }
}
