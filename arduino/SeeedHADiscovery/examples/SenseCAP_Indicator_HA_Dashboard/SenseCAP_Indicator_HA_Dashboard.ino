#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "DashboardUi.h"
#include "SenseCapIndicatorBus.h"
#include "SenseCapIndicatorTouch.h"

namespace {

constexpr int16_t kDisplayWidth = 480;
constexpr int16_t kDisplayHeight = 480;
constexpr uint16_t kLvglBufferRows = 40;
constexpr uint8_t kTouchRotation = 0;

static_assert(LV_COLOR_DEPTH == 16,
              "This dashboard requires LVGL RGB565 color output");

constexpr int8_t kI2cSdaPin = 39;
constexpr int8_t kI2cSclPin = 40;
constexpr uint32_t kI2cFrequency = 400000;
constexpr int8_t kLcdClockPin = 41;
constexpr int8_t kLcdDataPin = 48;
constexpr int8_t kBacklightPin = 45;

constexpr PCA95x5::Port::Port kLcdChipSelectPort = PCA95x5::Port::P04;
constexpr PCA95x5::Port::Port kLcdResetPort = PCA95x5::Port::P05;
constexpr PCA95x5::Port::Port kTouchInterruptPort = PCA95x5::Port::P06;
constexpr PCA95x5::Port::Port kTouchResetPort = PCA95x5::Port::P07;

PCA9555 ioExpander;
SenseCapIndicatorBus lcdBus(ioExpander, kLcdChipSelectPort,
                            kLcdClockPin, kLcdDataPin);
SenseCapIndicatorTouch touch(Wire, ioExpander, kTouchResetPort,
                             kI2cSdaPin, kI2cSclPin, kI2cFrequency,
                             kDisplayWidth, kDisplayHeight);

Arduino_ESP32RGBPanel rgbPanel(
    18, 17, 16, 21,
    4, 3, 2, 1, 0,
    10, 9, 8, 7, 6, 5,
    15, 14, 13, 12, 11,
    1, 10, 8, 50,
    1, 10, 8, 20);

Arduino_RGB_Display display(
    kDisplayWidth, kDisplayHeight, &rgbPanel, 2, true,
    &lcdBus, GFX_NOT_DEFINED, st7701_type1_init_operations,
    sizeof(st7701_type1_init_operations));

lv_display_t* lvglDisplay = nullptr;
lv_color_t* lvglDrawBuffer = nullptr;
bool lvglReady = false;

// Prepares the PCA9555 outputs that reset the LCD and touch controller.
// 准备用于复位 LCD 和触摸控制器的 PCA9555 输出引脚。
bool initializeDisplayControl() {
  Wire.begin(kI2cSdaPin, kI2cSclPin, kI2cFrequency);
  ioExpander.attach(Wire);

  if (!ioExpander.polarity(PCA95x5::Polarity::ORIGINAL_ALL)) {
    return false;
  }
  if (!ioExpander.write(kLcdChipSelectPort, PCA95x5::Level::H) ||
      !ioExpander.write(kLcdResetPort, PCA95x5::Level::L) ||
      !ioExpander.write(kTouchResetPort, PCA95x5::Level::L)) {
    return false;
  }
  if (!ioExpander.direction(kLcdChipSelectPort, PCA95x5::Direction::OUT) ||
      !ioExpander.direction(kLcdResetPort, PCA95x5::Direction::OUT) ||
      !ioExpander.direction(kTouchResetPort, PCA95x5::Direction::OUT) ||
      !ioExpander.direction(kTouchInterruptPort, PCA95x5::Direction::IN)) {
    return false;
  }

  delay(20);
  if (!ioExpander.write(kLcdResetPort, PCA95x5::Level::H) ||
      !ioExpander.write(kTouchResetPort, PCA95x5::Level::H)) {
    return false;
  }
  delay(120);
  return true;
}

// Copies an LVGL partial render area into the RGB display framebuffer.
// 将 LVGL 的局部渲染区域复制到 RGB 屏幕帧缓冲区。
void flushLvglDisplay(lv_display_t* lvDisplay, const lv_area_t* area,
                      uint8_t* pixelMap) {
  const uint32_t width = lv_area_get_width(area);
  const uint32_t height = lv_area_get_height(area);
  display.draw16bitRGBBitmap(area->x1, area->y1,
                             reinterpret_cast<uint16_t*>(pixelMap),
                             width, height);
  lv_display_flush_ready(lvDisplay);
}

// Supplies the latest transformed touch point to LVGL.
// 向 LVGL 提供经过方向映射的最新触摸坐标。
void readLvglTouch(lv_indev_t* inputDevice, lv_indev_data_t* data) {
  (void)inputDevice;
  uint16_t x = 0;
  uint16_t y = 0;

  if (touch.read(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Creates the LVGL display, partial draw buffer, and pointer input device.
// 创建 LVGL 显示器、局部绘图缓冲区和指针输入设备。
bool initializeLvgl() {
  lv_init();
  lv_tick_set_cb(millis);

  const size_t bufferPixels = kDisplayWidth * kLvglBufferRows;
  lvglDrawBuffer = static_cast<lv_color_t*>(heap_caps_malloc(
      bufferPixels * sizeof(lv_color_t),
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (lvglDrawBuffer == nullptr) {
    Serial.println("LVGL draw buffer allocation failed");
    return false;
  }

  lvglDisplay = lv_display_create(kDisplayWidth, kDisplayHeight);
  if (lvglDisplay == nullptr) {
    Serial.println("LVGL display creation failed");
    return false;
  }
  lv_display_set_color_format(lvglDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(lvglDisplay, flushLvglDisplay);
  lv_display_set_buffers(lvglDisplay, lvglDrawBuffer, nullptr,
                         bufferPixels * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* inputDevice = lv_indev_create();
  if (inputDevice == nullptr) {
    Serial.println("LVGL input device creation failed");
    return false;
  }
  lv_indev_set_type(inputDevice, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(inputDevice, lvglDisplay);
  lv_indev_set_read_cb(inputDevice, readLvglTouch);
  return true;
}

void printMemoryStatus() {
  Serial.printf("PSRAM total: %u bytes\n", ESP.getPsramSize());
  Serial.printf("PSRAM free: %u bytes\n", ESP.getFreePsram());
  Serial.printf("Internal heap free: %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("SenseCAP Indicator dashboard starting");

  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, LOW);

  if (!psramFound()) {
    Serial.println("PSRAM not detected");
    return;
  }
  printMemoryStatus();

  if (!initializeDisplayControl()) {
    Serial.printf("Display control initialization failed: I2C error %u\n",
                  ioExpander.i2c_error());
    return;
  }
  if (!display.begin(12000000L)) {
    Serial.println("Display initialization failed");
    return;
  }
  display.fillScreen(RGB565_BLACK);

  const bool touchReady = touch.begin(kTouchRotation);
  Wire.setClock(kI2cFrequency);
  Serial.println(touchReady ? "Touch controller ready"
                            : "Touch controller not detected");

  if (!initializeLvgl()) {
    return;
  }
  dashboardUiCreate();
  dashboardUiSetTouchAvailable(touchReady);
  lvglReady = true;

  digitalWrite(kBacklightPin, HIGH);
  Serial.println("LVGL dashboard ready");
}

void loop() {
  static uint32_t lastHeartbeatAt = 0;

  if (!lvglReady) {
    delay(100);
    return;
  }
  lv_timer_handler();
  const uint32_t now = millis();
  if (now - lastHeartbeatAt >= 2000) {
    Serial.println("Dashboard heartbeat");
    lastHeartbeatAt = now;
  }
  delay(5);
}
