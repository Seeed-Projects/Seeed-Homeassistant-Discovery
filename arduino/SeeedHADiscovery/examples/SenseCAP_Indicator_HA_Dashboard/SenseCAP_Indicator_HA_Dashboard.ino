#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include <SeeedHADiscovery.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "DashboardUi.h"
#include "RoomDashboardConfig.h"
#include "RoomDashboardState.h"
#include "SenseCapIndicatorBus.h"
#include "SenseCapIndicatorDisplay.h"
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
constexpr const char* kProvisioningAddress = "192.168.4.1";
constexpr uint32_t kNetworkStartupDelayMs = 2000;
constexpr uint32_t kNetworkTaskStackSize = 12288;
constexpr const char* kEntityCommandType = "ha_entity_command";
constexpr const char* kEntityCommandResultType =
    "ha_entity_command_result";

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

constexpr SenseCapIndicatorRgbPins kRgbPins = {
    16, 17, 18, 21,
    {4, 3, 2, 1, 0},
    {10, 9, 8, 7, 6, 5},
    {15, 14, 13, 12, 11},
};

constexpr SenseCapIndicatorRgbTiming kRgbTiming = {
    12000000L,
    10, 8, 50,
    10, 8, 20,
};

SenseCapIndicatorDisplay display(
    kDisplayWidth, kDisplayHeight, lcdBus,
    st7701_type1_init_operations,
    sizeof(st7701_type1_init_operations), kRgbPins, kRgbTiming);

lv_display_t* lvglDisplay = nullptr;
lv_color_t* lvglDrawBuffer = nullptr;
bool lvglReady = false;
bool connectionStateInitialized = false;
bool provisioningOverlayVisible = false;
DashboardConnectionState lastConnectionState =
    DashboardConnectionState::Offline;
SeeedHADiscovery* ha = nullptr;
TaskHandle_t networkTaskHandle = nullptr;
uint32_t dashboardReadyAt = 0;
uint32_t nextEntityCommandId = 1;

enum class NetworkStartupState : uint8_t {
  Idle,
  Starting,
  Ready,
  Failed,
};

volatile NetworkStartupState networkStartupState = NetworkStartupState::Idle;

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

// Converts network flags into one stable status shared by all UI pages.
// 将网络标志转换成所有 UI 页面共用的一种稳定状态。
DashboardConnectionState getConnectionState() {
  if (networkStartupState != NetworkStartupState::Ready || ha == nullptr) {
    return DashboardConnectionState::Offline;
  }
  if (ha->isProvisioningActive()) {
    return DashboardConnectionState::Provisioning;
  }
  if (!ha->isWiFiConnected()) {
    return DashboardConnectionState::Offline;
  }
  return ha->isHAConnected() ? DashboardConnectionState::Online
                             : DashboardConnectionState::WaitingForHA;
}

// Refreshes connection labels and the first-boot provisioning guide.
// 刷新连接标签，并在首次启动时显示配网引导。
void updateConnectionUi() {
  const DashboardConnectionState state = getConnectionState();
  if (!connectionStateInitialized || state != lastConnectionState) {
    dashboardUiSetConnectionState(state);
    dashboardUiSetControlsEnabled(
        state == DashboardConnectionState::Online);
    lastConnectionState = state;
    connectionStateInitialized = true;
    Serial.printf("Dashboard connection state: %u\n",
                  static_cast<uint8_t>(state));
  }

  const bool provisioning = state == DashboardConnectionState::Provisioning;
  if (provisioning != provisioningOverlayVisible) {
    dashboardUiSetProvisioningState(provisioning,
                                    kDashboardProvisioningAp,
                                    kProvisioningAddress);
    provisioningOverlayVisible = provisioning;
  }
}

// Sends one authorized entity action through the HA integration.
// 通过 HA 集成发送一个已授权的实体动作。
void sendEntityCommand(const char* action, const char* const* entityIds,
                       size_t entityCount) {
  if (ha == nullptr || !ha->isHAConnected()) {
    dashboardUiShowNotice("HA unavailable");
    return;
  }

  JsonDocument document;
  document["type"] = kEntityCommandType;
  document["request_id"] = nextEntityCommandId++;
  document["action"] = action;
  JsonArray entities = document["entity_ids"].to<JsonArray>();
  for (size_t index = 0; index < entityCount; ++index) {
    entities.add(entityIds[index]);
  }
  ha->sendProtocolMessage(document);
}

void handleDashboardAction(DashboardAction action) {
  if (action == DashboardAction::WindowToggle) {
    const char* entities[] = {kWindowEntity};
    sendEntityCommand("toggle", entities, 1);
  } else if (action == DashboardAction::TvPowerToggle) {
    const char* entities[] = {kTvPowerEntity};
    sendEntityCommand("toggle", entities, 1);
  } else if (action == DashboardAction::LeaveRoom) {
    sendEntityCommand("turn_off", kLeaveRoomEntities,
                      kLeaveRoomEntityCount);
  }
}

// Starts blocking WiFi provisioning on the other CPU core.
// 在另一个 CPU 核心上启动会阻塞的 WiFi 配网流程。
void runNetworkStartup(void* parameter) {
  (void)parameter;
  Serial0.println("Network stage: HA object creating");

  SeeedHADiscovery* instance = new SeeedHADiscovery();
  if (instance == nullptr) {
    networkStartupState = NetworkStartupState::Failed;
    Serial0.println("Network stopped: HA allocation failed");
    networkTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  instance->enableDebug(true);
  instance->setDeviceInfo("SenseCAP Indicator Room Dashboard",
                          "SenseCAP Indicator", "1.0.0");
  instance->onHAState([](const char* entityId, const char* state,
                         JsonObject& attributes) {
    roomDashboardStateUpdate(entityId, state, attributes);
  });
  instance->onProtocolMessage(
      kEntityCommandResultType, [](JsonDocument& document) {
        const bool success = document["success"] | false;
        const char* error = document["error"] | "";
        if (success) {
          dashboardUiShowNotice("HA action completed");
        } else if (strcmp(error, "entity_not_subscribed") == 0) {
          dashboardUiShowNotice("Select entity in HA");
        } else {
          dashboardUiShowNotice("HA action failed");
        }
        Serial.printf("HA action result: success=%s, error=%s\n",
                      success ? "true" : "false", error);
      });

  Serial0.println("Network stage: WiFi provisioning starting");
  const bool wifiConnected =
      instance->beginWithProvisioning(kDashboardProvisioningAp);

  ha = instance;
  networkStartupState = NetworkStartupState::Ready;
  Serial0.println(wifiConnected
                      ? "Network stage: WiFi connected"
                      : "Network stage: provisioning hotspot ready");
  networkTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

// Gives LVGL time to finish its first frame before WiFi scanning begins.
// 先让 LVGL 完成首帧，再开始 WiFi 扫描。
void startNetworkWhenDisplayIsReady() {
  if (networkStartupState != NetworkStartupState::Idle ||
      millis() - dashboardReadyAt < kNetworkStartupDelayMs) {
    return;
  }

  networkStartupState = NetworkStartupState::Starting;
  const BaseType_t result = xTaskCreatePinnedToCore(
      runNetworkStartup, "NetworkStartup", kNetworkTaskStackSize,
      nullptr, 1, &networkTaskHandle, 0);
  if (result != pdPASS) {
    networkTaskHandle = nullptr;
    networkStartupState = NetworkStartupState::Failed;
    Serial0.println("Network stopped: task creation failed");
  }
}

}  // namespace

void setup() {
  Serial0.begin(115200);
  Serial.begin(115200);
  delay(500);
  Serial0.println("Boot stage: setup entered");
  Serial.println("SenseCAP Indicator dashboard starting");

  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, LOW);

  if (!psramFound()) {
    Serial0.println("Boot stopped: PSRAM not detected");
    Serial.println("PSRAM not detected");
    return;
  }
  Serial0.println("Boot stage: PSRAM ready");
  printMemoryStatus();

  // Initializes the WiFi driver before the RGB panel starts continuous DMA.
  // 在 RGB 屏幕启动连续 DMA 之前初始化 WiFi 驱动。
  Serial0.println("Boot stage: WiFi driver starting");
  if (!WiFi.mode(WIFI_STA)) {
    Serial0.println("Boot stopped: WiFi driver failed");
    return;
  }
  Serial0.println("Boot stage: WiFi driver ready");

  if (!initializeDisplayControl()) {
    Serial0.println("Boot stopped: display control failed");
    Serial.printf("Display control initialization failed: I2C error %u\n",
                  ioExpander.i2c_error());
    return;
  }
  if (!display.begin()) {
    Serial0.println("Boot stopped: display initialization failed");
    Serial.println("Display initialization failed");
    return;
  }
  display.fillScreen(RGB565_BLACK);

  const bool touchReady = touch.begin(kTouchRotation);
  Wire.setClock(kI2cFrequency);
  Serial.println(touchReady ? "Touch controller ready"
                            : "Touch controller not detected");

  if (!initializeLvgl()) {
    Serial0.println("Boot stopped: LVGL initialization failed");
    return;
  }
  dashboardUiCreate();
  dashboardUiSetRoomName(kDashboardRoomName);
  dashboardUiSetTouchAvailable(touchReady);
  dashboardUiSetControlsEnabled(false);
  dashboardUiOnAction(handleDashboardAction);
  lvglReady = true;

  digitalWrite(kBacklightPin, HIGH);
  dashboardReadyAt = millis();
  Serial0.println("Boot stage: dashboard ready");
  Serial.println("LVGL dashboard ready");
}

void loop() {
  static uint32_t lastStatusAt = 0;

  if (!lvglReady) {
    delay(100);
    return;
  }
  startNetworkWhenDisplayIsReady();
  if (networkStartupState == NetworkStartupState::Ready && ha != nullptr) {
    ha->handle();
  }
  updateConnectionUi();
  roomDashboardStateApply();
  lv_timer_handler();
  const uint32_t now = millis();
  if (now - lastStatusAt >= 10000) {
    Serial.printf(
        "Dashboard status: WiFi=%s, HA=%s, provisioning=%s\n",
        ha != nullptr && ha->isWiFiConnected() ? "connected" : "disconnected",
        ha != nullptr && ha->isHAConnected() ? "connected" : "disconnected",
        ha != nullptr && ha->isProvisioningActive() ? "active" : "inactive");
    lastStatusAt = now;
  }
  delay(5);
}
