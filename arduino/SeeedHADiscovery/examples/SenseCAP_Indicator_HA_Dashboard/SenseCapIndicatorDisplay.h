#pragma once

#include <Arduino_DataBus.h>
#include <esp_lcd_panel_rgb.h>

struct SenseCapIndicatorRgbPins {
  int hsync;
  int vsync;
  int dataEnable;
  int pixelClock;
  int red[5];
  int green[6];
  int blue[5];
};

struct SenseCapIndicatorRgbTiming {
  uint32_t pixelClockHz;
  uint16_t hsyncFrontPorch;
  uint16_t hsyncPulseWidth;
  uint16_t hsyncBackPorch;
  uint16_t vsyncFrontPorch;
  uint16_t vsyncPulseWidth;
  uint16_t vsyncBackPorch;
};

class SenseCapIndicatorDisplay {
 public:
  SenseCapIndicatorDisplay(
      uint16_t width, uint16_t height, Arduino_DataBus& commandBus,
      const uint8_t* initOperations, size_t initOperationsLength,
      const SenseCapIndicatorRgbPins& pins,
      const SenseCapIndicatorRgbTiming& timing);

  bool begin();
  void fillScreen(uint16_t color);
  void draw16bitRGBBitmap(int16_t x, int16_t y, const uint16_t* bitmap,
                          int16_t width, int16_t height);

 private:
  // Writes changed PSRAM rows back so RGB DMA sees the latest pixels.
  // 将已修改的 PSRAM 行写回，确保 RGB DMA 读取到最新像素。
  void writeBackRows(uint16_t firstRow, uint16_t rowCount);

  uint16_t width_;
  uint16_t height_;
  Arduino_DataBus& commandBus_;
  const uint8_t* initOperations_;
  size_t initOperationsLength_;
  SenseCapIndicatorRgbPins pins_;
  SenseCapIndicatorRgbTiming timing_;
  esp_lcd_panel_handle_t panel_;
  uint16_t* frameBuffer_;
};
