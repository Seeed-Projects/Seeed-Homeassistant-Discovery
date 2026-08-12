#include "SenseCapIndicatorDisplay.h"

#include <esp_lcd_panel_ops.h>
#include <esp32s3/rom/cache.h>

SenseCapIndicatorDisplay::SenseCapIndicatorDisplay(
    uint16_t width, uint16_t height, Arduino_DataBus& commandBus,
    const uint8_t* initOperations, size_t initOperationsLength,
    const SenseCapIndicatorRgbPins& pins,
    const SenseCapIndicatorRgbTiming& timing)
    : width_(width),
      height_(height),
      commandBus_(commandBus),
      initOperations_(initOperations),
      initOperationsLength_(initOperationsLength),
      pins_(pins),
      timing_(timing),
      panel_(nullptr),
      frameBuffer_(nullptr) {}

bool SenseCapIndicatorDisplay::begin() {
  if (!commandBus_.begin()) {
    return false;
  }

  commandBus_.sendCommand(0x01);
  delay(120);
  commandBus_.batchOperation(const_cast<uint8_t*>(initOperations_),
                             initOperationsLength_);

  esp_lcd_rgb_panel_config_t panelConfig = {};
  panelConfig.clk_src = LCD_CLK_SRC_DEFAULT;
  panelConfig.timings.pclk_hz = timing_.pixelClockHz;
  panelConfig.timings.h_res = width_;
  panelConfig.timings.v_res = height_;
  panelConfig.timings.hsync_pulse_width = timing_.hsyncPulseWidth;
  panelConfig.timings.hsync_back_porch = timing_.hsyncBackPorch;
  panelConfig.timings.hsync_front_porch = timing_.hsyncFrontPorch;
  panelConfig.timings.vsync_pulse_width = timing_.vsyncPulseWidth;
  panelConfig.timings.vsync_back_porch = timing_.vsyncBackPorch;
  panelConfig.timings.vsync_front_porch = timing_.vsyncFrontPorch;
  panelConfig.timings.flags.hsync_idle_low = 0;
  panelConfig.timings.flags.vsync_idle_low = 0;
  panelConfig.timings.flags.de_idle_high = 0;
  panelConfig.timings.flags.pclk_active_neg = 0;
  panelConfig.timings.flags.pclk_idle_high = 0;
  panelConfig.data_width = 16;
  panelConfig.bits_per_pixel = 16;
  panelConfig.num_fbs = 1;
  panelConfig.bounce_buffer_size_px = 0;
  panelConfig.dma_burst_size = 64;
  panelConfig.hsync_gpio_num = pins_.hsync;
  panelConfig.vsync_gpio_num = pins_.vsync;
  panelConfig.de_gpio_num = pins_.dataEnable;
  panelConfig.pclk_gpio_num = pins_.pixelClock;
  panelConfig.disp_gpio_num = -1;

  for (uint8_t index = 0; index < 5; ++index) {
    panelConfig.data_gpio_nums[index] = pins_.blue[index];
  }
  for (uint8_t index = 0; index < 6; ++index) {
    panelConfig.data_gpio_nums[index + 5] = pins_.green[index];
  }
  for (uint8_t index = 0; index < 5; ++index) {
    panelConfig.data_gpio_nums[index + 11] = pins_.red[index];
  }

  panelConfig.flags.disp_active_low = 1;
  panelConfig.flags.refresh_on_demand = 0;
  panelConfig.flags.fb_in_psram = 1;
  panelConfig.flags.double_fb = 0;
  panelConfig.flags.no_fb = 0;
  panelConfig.flags.bb_invalidate_cache = 0;

  if (esp_lcd_new_rgb_panel(&panelConfig, &panel_) != ESP_OK ||
      esp_lcd_panel_reset(panel_) != ESP_OK ||
      esp_lcd_panel_init(panel_) != ESP_OK) {
    return false;
  }

  void* frameBuffer = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(panel_, 1, &frameBuffer) != ESP_OK) {
    return false;
  }
  frameBuffer_ = static_cast<uint16_t*>(frameBuffer);
  return frameBuffer_ != nullptr;
}

void SenseCapIndicatorDisplay::fillScreen(uint16_t color) {
  if (frameBuffer_ == nullptr) {
    return;
  }

  const size_t pixelCount = static_cast<size_t>(width_) * height_;
  for (size_t index = 0; index < pixelCount; ++index) {
    frameBuffer_[index] = color;
  }
  writeBackRows(0, height_);
}

void SenseCapIndicatorDisplay::draw16bitRGBBitmap(
    int16_t x, int16_t y, const uint16_t* bitmap,
    int16_t width, int16_t height) {
  if (frameBuffer_ == nullptr || bitmap == nullptr || width <= 0 ||
      height <= 0 || x < 0 || y < 0 || x + width > width_ ||
      y + height > height_) {
    return;
  }

  // Rotate the LVGL area by 180 degrees to match the installed panel.
  // 将 LVGL 区域旋转 180 度，以匹配屏幕的安装方向。
  for (int16_t sourceY = 0; sourceY < height; ++sourceY) {
    const uint16_t targetY = height_ - 1 - (y + sourceY);
    uint16_t* target = frameBuffer_ +
                       static_cast<size_t>(targetY) * width_ +
                       (width_ - x - width);
    const uint16_t* source = bitmap +
                             static_cast<size_t>(sourceY) * width;
    for (int16_t sourceX = 0; sourceX < width; ++sourceX) {
      target[width - 1 - sourceX] = source[sourceX];
    }
  }

  writeBackRows(height_ - y - height, height);
}

void SenseCapIndicatorDisplay::writeBackRows(uint16_t firstRow,
                                              uint16_t rowCount) {
  uint16_t* firstPixel = frameBuffer_ +
                         static_cast<size_t>(firstRow) * width_;
  Cache_WriteBack_Addr(reinterpret_cast<uint32_t>(firstPixel),
                       static_cast<uint32_t>(rowCount) * width_ *
                           sizeof(uint16_t));
}
