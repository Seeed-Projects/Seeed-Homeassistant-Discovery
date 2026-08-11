#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include <Wire.h>

#include "SenseCapIndicatorBus.h"

namespace {

constexpr int16_t kDisplayWidth = 480;
constexpr int16_t kDisplayHeight = 480;

constexpr int8_t kI2cSdaPin = 39;
constexpr int8_t kI2cSclPin = 40;
constexpr uint32_t kI2cFrequency = 400000;
constexpr int8_t kLcdClockPin = 41;
constexpr int8_t kLcdDataPin = 48;
constexpr int8_t kBacklightPin = 45;

constexpr PCA95x5::Port::Port kLcdChipSelectPort = PCA95x5::Port::P04;
constexpr PCA95x5::Port::Port kLcdResetPort = PCA95x5::Port::P05;

constexpr uint16_t kBackgroundColor = 0x18E3;
constexpr uint16_t kPanelColor = 0x2986;
constexpr uint16_t kPrimaryTextColor = 0xFFFF;
constexpr uint16_t kSecondaryTextColor = 0xBDF7;
constexpr uint16_t kReadyColor = 0x4E69;
constexpr uint16_t kRedColor = 0xF945;
constexpr uint16_t kGreenColor = 0x4E69;
constexpr uint16_t kBlueColor = 0x4D7F;
constexpr uint16_t kWhiteColor = 0xFFFF;

PCA9555 ioExpander;
SenseCapIndicatorBus lcdBus(ioExpander, kLcdChipSelectPort,
                            kLcdClockPin, kLcdDataPin);

Arduino_ESP32RGBPanel rgbPanel(
    18, 17, 16, 21,
    4, 3, 2, 1, 0,
    10, 9, 8, 7, 6, 5,
    15, 14, 13, 12, 11,
    1, 10, 8, 50,
    1, 10, 8, 20);

Arduino_RGB_Display display(
    kDisplayWidth, kDisplayHeight, &rgbPanel, 0, true,
    &lcdBus, GFX_NOT_DEFINED, st7701_type1_init_operations,
    sizeof(st7701_type1_init_operations));

// Prepares the PCA9555 outputs that control LCD chip select and reset.
// 准备用于控制 LCD 片选和复位的 PCA9555 输出引脚。
bool initializeDisplayControl() {
  Wire.begin(kI2cSdaPin, kI2cSclPin, kI2cFrequency);
  ioExpander.attach(Wire);

  if (!ioExpander.polarity(PCA95x5::Polarity::ORIGINAL_ALL)) {
    return false;
  }
  if (!ioExpander.write(kLcdChipSelectPort, PCA95x5::Level::H) ||
      !ioExpander.write(kLcdResetPort, PCA95x5::Level::L)) {
    return false;
  }
  if (!ioExpander.direction(kLcdChipSelectPort, PCA95x5::Direction::OUT) ||
      !ioExpander.direction(kLcdResetPort, PCA95x5::Direction::OUT)) {
    return false;
  }

  delay(20);
  if (!ioExpander.write(kLcdResetPort, PCA95x5::Level::H)) {
    return false;
  }
  delay(120);
  return true;
}

// Draws text centered on the horizontal axis at the requested baseline area.
// 在指定的纵向位置绘制水平居中文字。
void drawCenteredText(const char* text, int16_t y, uint8_t size,
                      uint16_t color) {
  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t boundsWidth = 0;
  uint16_t boundsHeight = 0;

  display.setTextSize(size);
  display.getTextBounds(text, 0, y, &boundsX, &boundsY,
                        &boundsWidth, &boundsHeight);
  display.setTextColor(color);
  display.setCursor((kDisplayWidth - boundsWidth) / 2, y);
  display.print(text);
}

// Draws the phase-one hardware verification screen.
// 绘制第一阶段的硬件验证画面。
void drawDisplayTestScreen() {
  display.fillScreen(kBackgroundColor);
  display.fillRoundRect(28, 30, 424, 330, 28, kPanelColor);

  drawCenteredText("SenseCAP Indicator", 78, 3, kPrimaryTextColor);
  drawCenteredText("Display Ready", 158, 4, kReadyColor);
  drawCenteredText("480 x 480", 230, 2, kSecondaryTextColor);

  constexpr int16_t kBlockY = 390;
  constexpr int16_t kBlockWidth = 88;
  constexpr int16_t kBlockHeight = 56;
  constexpr int16_t kBlockGap = 16;
  constexpr int16_t kBlockStartX = 40;
  const uint16_t colors[] = {
      kRedColor, kGreenColor, kBlueColor, kWhiteColor};

  for (uint8_t index = 0; index < 4; ++index) {
    const int16_t x = kBlockStartX + index * (kBlockWidth + kBlockGap);
    display.fillRoundRect(x, kBlockY, kBlockWidth, kBlockHeight, 12,
                          colors[index]);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("SenseCAP Indicator display test starting");

  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, LOW);

  if (!initializeDisplayControl()) {
    Serial.printf("Display control initialization failed: I2C error %u\n",
                  ioExpander.i2c_error());
    return;
  }

  if (!display.begin(12000000L)) {
    Serial.println("Display initialization failed");
    return;
  }

  drawDisplayTestScreen();
  digitalWrite(kBacklightPin, HIGH);
  Serial.println("Display test screen ready");
}

void loop() {
  delay(1000);
}
