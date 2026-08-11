/**
 * ============================================================================
 * XIAO ESP32-C3 + SCD41 Air Quality Display
 * XIAO ESP32-C3 + SCD41 空气质量显示
 * ============================================================================
 *
 * Displays SCD41 measurements on a 1.47-inch ST7789 LCD and publishes
 * CO2, temperature, and humidity to Home Assistant over WiFi.
 * 在 1.47 英寸 ST7789 LCD 上显示 SCD41 测量值，并通过 WiFi
 * 将二氧化碳、温度和湿度发布到 Home Assistant。
 *
 * @author Seeed Studio
 * @version 1.1.0
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SensirionI2cScd4x.h>
#include <SeeedHADiscovery.h>
#include <SPI.h>
#include <Wire.h>

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

// LCD SPI pins | LCD SPI 引脚
constexpr int8_t LCD_RST_PIN = D0;
constexpr int8_t LCD_CS_PIN = D1;
constexpr int8_t LCD_DC_PIN = D3;
constexpr int8_t LCD_BL_PIN = D6;
constexpr int8_t LCD_SCK_PIN = D8;
constexpr int8_t LCD_MOSI_PIN = D10;

// SCD41 I2C pins | SCD41 I2C 引脚
constexpr int8_t SENSOR_SDA_PIN = D4;
constexpr int8_t SENSOR_SCL_PIN = D5;

// WiFi provisioning access point | WiFi 配网热点
const char* AP_SSID = "Seeed_AirMonitor_AP";
// BOOT button for WiFi reprovisioning | 用于重新配网的 BOOT 按键
constexpr int8_t RESET_BUTTON_PIN = D9;

// Sensor timing | 传感器时间配置
constexpr unsigned long SENSOR_POLL_INTERVAL_MS = 500;
constexpr unsigned long SENSOR_RETRY_INTERVAL_MS = 5000;

// UI colors in RGB565 format | RGB565 格式的界面颜色
constexpr uint16_t COLOR_BACKGROUND = 0x0842;
constexpr uint16_t COLOR_SURFACE = 0x10A4;
constexpr uint16_t COLOR_SURFACE_ALT = 0x18E6;
constexpr uint16_t COLOR_BORDER = 0x294A;
constexpr uint16_t COLOR_TEXT_PRIMARY = 0xFFFF;
constexpr uint16_t COLOR_TEXT_SECONDARY = 0xAD75;
constexpr uint16_t COLOR_INFO = 0x4D7F;
constexpr uint16_t COLOR_GOOD = 0x2E66;
constexpr uint16_t COLOR_FAIR = 0xFE60;
constexpr uint16_t COLOR_POOR = 0xFB20;
constexpr uint16_t COLOR_BAD = 0xF2A6;

struct AirQualityStyle {
    const char* label;
    uint16_t color;
};

enum class ConnectionBadgeState : uint8_t {
    Unknown,
    Setup,
    Offline,
    Wifi,
    HomeAssistantOnline,
};

Adafruit_ST7789 display(&SPI, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN);
SensirionI2cScd4x scd41;
SeeedHADiscovery ha;

SeeedHASensor* co2Sensor = nullptr;
SeeedHASensor* temperatureSensor = nullptr;
SeeedHASensor* humiditySensor = nullptr;

bool sensorRunning = false;
bool homeAssistantSensorsReady = false;
unsigned long lastSensorPollMs = 0;
unsigned long lastSensorRetryMs = 0;
ConnectionBadgeState lastConnectionBadgeState =
    ConnectionBadgeState::Unknown;

/**
 * Draw text centered inside a rectangular area.
 * 在矩形区域内绘制居中文字。
 */
void drawCenteredText(const char* text, int16_t x, int16_t y, int16_t width,
                      int16_t height, uint8_t textSize, uint16_t color) {
    int16_t boundsX = 0;
    int16_t boundsY = 0;
    uint16_t boundsWidth = 0;
    uint16_t boundsHeight = 0;

    display.setTextSize(textSize);
    display.setTextColor(color);
    display.getTextBounds(text, 0, 0, &boundsX, &boundsY, &boundsWidth,
                          &boundsHeight);

    const int16_t cursorX = x + (width - boundsWidth) / 2 - boundsX;
    const int16_t cursorY = y + (height - boundsHeight) / 2 - boundsY;
    display.setCursor(cursorX, cursorY);
    display.print(text);
}

/**
 * Draw the connection-status badge in the header.
 * 绘制标题栏中的连接状态标签。
 */
void drawHeaderBadge(const char* label, uint16_t color) {
    constexpr int16_t badgeX = 240;
    constexpr int16_t badgeY = 9;
    constexpr int16_t badgeWidth = 72;
    constexpr int16_t badgeHeight = 22;

    display.fillRoundRect(badgeX, badgeY, badgeWidth, badgeHeight, 7,
                          COLOR_SURFACE_ALT);
    display.drawRoundRect(badgeX, badgeY, badgeWidth, badgeHeight, 7, color);
    drawCenteredText(label, badgeX, badgeY, badgeWidth, badgeHeight, 1, color);
}

/**
 * Update the header badge from the current WiFi and HA connection state.
 * 根据当前 WiFi 和 HA 连接状态更新标题栏标签。
 */
void updateConnectionBadge(bool forceRedraw = false) {
    ConnectionBadgeState currentState = ConnectionBadgeState::Offline;

    if (ha.isProvisioningActive()) {
        currentState = ConnectionBadgeState::Setup;
    } else if (!ha.isWiFiConnected()) {
        currentState = ConnectionBadgeState::Offline;
    } else if (ha.isHAConnected()) {
        currentState = ConnectionBadgeState::HomeAssistantOnline;
    } else {
        currentState = ConnectionBadgeState::Wifi;
    }

    if (!forceRedraw && currentState == lastConnectionBadgeState) {
        return;
    }

    lastConnectionBadgeState = currentState;
    switch (currentState) {
        case ConnectionBadgeState::Setup:
            drawHeaderBadge("SETUP", COLOR_FAIR);
            break;
        case ConnectionBadgeState::Offline:
            drawHeaderBadge("OFFLINE", COLOR_BAD);
            break;
        case ConnectionBadgeState::Wifi:
            drawHeaderBadge("WIFI", COLOR_INFO);
            break;
        case ConnectionBadgeState::HomeAssistantOnline:
            drawHeaderBadge("HA ONLINE", COLOR_GOOD);
            break;
        case ConnectionBadgeState::Unknown:
            drawHeaderBadge("START", COLOR_INFO);
            break;
    }
}

/**
 * Draw the fixed header and background.
 * 绘制固定的标题栏和背景。
 */
void drawStaticLayout() {
    display.fillScreen(COLOR_BACKGROUND);
    display.setTextWrap(false);
    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT_PRIMARY);
    display.setCursor(12, 12);
    display.print("AIR QUALITY");
    drawHeaderBadge("START", COLOR_INFO);
}

/**
 * Draw a centered message in the main content card.
 * 在主内容卡片中绘制居中消息。
 */
void drawMessageCard(const char* title, const char* subtitle,
                     uint16_t accentColor) {
    constexpr int16_t cardX = 8;
    constexpr int16_t cardY = 40;
    constexpr int16_t cardWidth = 304;
    constexpr int16_t cardHeight = 72;

    display.fillRoundRect(cardX, cardY, cardWidth, cardHeight, 12,
                          COLOR_SURFACE);
    display.drawRoundRect(cardX, cardY, cardWidth, cardHeight, 12,
                          COLOR_BORDER);
    drawCenteredText(title, cardX, cardY + 11, cardWidth, 28, 2, accentColor);
    drawCenteredText(subtitle, cardX, cardY + 42, cardWidth, 18, 1,
                     COLOR_TEXT_SECONDARY);
}

/**
 * Draw a metric card with a placeholder value.
 * 绘制带占位数值的指标卡片。
 */
void drawMetricPlaceholder(int16_t x, const char* label) {
    constexpr int16_t cardY = 120;
    constexpr int16_t cardWidth = 148;
    constexpr int16_t cardHeight = 44;

    display.fillRoundRect(x, cardY, cardWidth, cardHeight, 10,
                          COLOR_SURFACE_ALT);
    display.drawRoundRect(x, cardY, cardWidth, cardHeight, 10, COLOR_BORDER);
    display.setTextSize(1);
    display.setTextColor(COLOR_TEXT_SECONDARY);
    display.setCursor(x + 12, cardY + 8);
    display.print(label);
    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT_PRIMARY);
    display.setCursor(x + 12, cardY + 23);
    display.print("--");
}

/**
 * Draw a metric card with one decimal place.
 * 绘制保留一位小数的指标卡片。
 */
void drawMetricCard(int16_t x, const char* label, float value,
                    const char* unit, uint16_t accentColor) {
    constexpr int16_t cardY = 120;
    constexpr int16_t cardWidth = 148;
    constexpr int16_t cardHeight = 44;
    char valueText[20];

    snprintf(valueText, sizeof(valueText), "%.1f %s", value, unit);
    display.fillRoundRect(x, cardY, cardWidth, cardHeight, 10,
                          COLOR_SURFACE_ALT);
    display.drawRoundRect(x, cardY, cardWidth, cardHeight, 10, COLOR_BORDER);
    display.fillRoundRect(x + 8, cardY + 7, 4, 30, 2, accentColor);
    display.setTextSize(1);
    display.setTextColor(COLOR_TEXT_SECONDARY);
    display.setCursor(x + 20, cardY + 7);
    display.print(label);
    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT_PRIMARY);
    display.setCursor(x + 20, cardY + 21);
    display.print(valueText);
}

/**
 * Return the UI style for a CO2 concentration.
 * 返回二氧化碳浓度对应的界面样式。
 */
AirQualityStyle classifyAirQuality(uint16_t co2Ppm) {
    if (co2Ppm <= 800) {
        return {"GOOD", COLOR_GOOD};
    }
    if (co2Ppm <= 1000) {
        return {"FAIR", COLOR_FAIR};
    }
    if (co2Ppm <= 1500) {
        return {"POOR", COLOR_POOR};
    }
    return {"BAD", COLOR_BAD};
}

/**
 * Draw the latest SCD41 measurement.
 * 绘制最新的 SCD41 测量数据。
 */
void drawMeasurement(uint16_t co2Ppm, float temperature,
                     float relativeHumidity) {
    constexpr int16_t cardX = 8;
    constexpr int16_t cardY = 40;
    constexpr int16_t cardWidth = 304;
    constexpr int16_t cardHeight = 72;
    constexpr int16_t qualityX = 18;
    constexpr int16_t qualityY = 51;
    constexpr int16_t qualityWidth = 70;
    constexpr int16_t qualityHeight = 22;
    char co2Text[8];

    const AirQualityStyle style = classifyAirQuality(co2Ppm);
    snprintf(co2Text, sizeof(co2Text), "%u", co2Ppm);

    display.fillRoundRect(cardX, cardY, cardWidth, cardHeight, 12,
                          COLOR_SURFACE);
    display.drawRoundRect(cardX, cardY, cardWidth, cardHeight, 12,
                          COLOR_BORDER);

    display.fillRoundRect(qualityX, qualityY, qualityWidth, qualityHeight, 7,
                          style.color);
    drawCenteredText(style.label, qualityX, qualityY, qualityWidth,
                     qualityHeight, 1, COLOR_BACKGROUND);

    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT_SECONDARY);
    display.setCursor(32, 83);
    display.print("CO2");

    drawCenteredText(co2Text, 98, 48, 202, 45, 5, COLOR_TEXT_PRIMARY);
    drawCenteredText("ppm", 98, 91, 202, 14, 1, COLOR_TEXT_SECONDARY);

    drawMetricCard(8, "TEMPERATURE", temperature, "C", COLOR_INFO);
    drawMetricCard(164, "HUMIDITY", relativeHumidity, "%", COLOR_GOOD);
}

/**
 * Draw the sensor warm-up state.
 * 绘制传感器预热状态。
 */
void drawWaitingScreen() {
    drawMessageCard("WARMING UP", "FIRST READING IN ABOUT 5 SECONDS",
                    COLOR_INFO);
    drawMetricPlaceholder(8, "TEMPERATURE");
    drawMetricPlaceholder(164, "HUMIDITY");
}

/**
 * Draw the sensor error state.
 * 绘制传感器错误状态。
 */
void drawSensorError() {
    drawMessageCard("SENSOR ERROR", "CHECK POWER AND I2C WIRING", COLOR_BAD);
    drawMetricPlaceholder(8, "TEMPERATURE");
    drawMetricPlaceholder(164, "HUMIDITY");
}

/**
 * Print a Sensirion error to the serial monitor.
 * 将 Sensirion 错误输出到串口监视器。
 */
void printSensorError(const char* operation, int16_t error) {
    char errorMessage[64];
    errorToString(error, errorMessage, sizeof(errorMessage));
    Serial.print("SCD41 ");
    Serial.print(operation);
    Serial.print(" failed: ");
    Serial.println(errorMessage);
}

/**
 * Print the SCD41 serial number in hexadecimal format.
 * 以十六进制格式输出 SCD41 序列号。
 */
void printSensorSerialNumber(uint64_t serialNumber) {
    Serial.print("SCD41 serial number: 0x");
    Serial.print(static_cast<uint32_t>(serialNumber >> 32), HEX);
    Serial.println(static_cast<uint32_t>(serialNumber & 0xFFFFFFFF), HEX);
}

/**
 * Initialize the SCD41 and start periodic measurements.
 * 初始化 SCD41 并启动周期测量。
 */
bool initializeSensor() {
    uint64_t serialNumber = 0;
    int16_t error = NO_ERROR;

    scd41.begin(Wire, SCD41_I2C_ADDR_62);
    delay(30);

    error = scd41.wakeUp();
    if (error != NO_ERROR) {
        printSensorError("wake-up", error);
    }

    error = scd41.stopPeriodicMeasurement();
    if (error != NO_ERROR) {
        printSensorError("stop", error);
    }

    error = scd41.reinit();
    if (error != NO_ERROR) {
        printSensorError("reinitialization", error);
        return false;
    }

    error = scd41.getSerialNumber(serialNumber);
    if (error != NO_ERROR) {
        printSensorError("serial-number read", error);
        return false;
    }

    error = scd41.startPeriodicMeasurement();
    if (error != NO_ERROR) {
        printSensorError("measurement start", error);
        return false;
    }

    printSensorSerialNumber(serialNumber);
    Serial.println("SCD41 periodic measurement started");
    return true;
}

/**
 * Create the Home Assistant sensor entities.
 * 创建 Home Assistant 传感器实体。
 */
void registerHomeAssistantSensors() {
    co2Sensor =
        ha.addSensor("carbon_dioxide", "Carbon Dioxide", "carbon_dioxide",
                     "ppm");
    co2Sensor->setPrecision(0);
    co2Sensor->setIcon("mdi:molecule-co2");

    temperatureSensor =
        ha.addSensor("temperature", "Temperature", "temperature", "°C");
    temperatureSensor->setPrecision(1);

    humiditySensor = ha.addSensor("humidity", "Humidity", "humidity", "%");
    humiditySensor->setPrecision(1);

    homeAssistantSensorsReady = true;
    Serial.println("Home Assistant sensor entities registered");
}

/**
 * Start WiFi provisioning and Home Assistant discovery services.
 * 启动 WiFi 配网和 Home Assistant 发现服务。
 */
void initializeHomeAssistant() {
    ha.setDeviceInfo("SCD41 Air Quality Monitor", "XIAO ESP32-C3", "1.1.0");
    ha.enableDebug(true);
    ha.enableResetButton(RESET_BUTTON_PIN);

    Serial.print("WiFi reset button enabled on GPIO");
    Serial.println(RESET_BUTTON_PIN);
    Serial.println("Hold the BOOT button for 6 seconds to reconfigure WiFi");

    drawHeaderBadge("WIFI...", COLOR_INFO);
    const bool wifiConnected = ha.beginWithProvisioning(AP_SSID);

    if (!wifiConnected) {
        Serial.println("WiFi provisioning is active");
        Serial.print("Connect to access point: ");
        Serial.println(AP_SSID);
        Serial.println("Open: http://192.168.4.1");
        updateConnectionBadge(true);
        return;
    }

    registerHomeAssistantSensors();
    Serial.print("WiFi connected, device IP: ");
    Serial.println(ha.getLocalIP());
    Serial.println("Add the Seeed HA Discovery integration in Home Assistant");
    updateConnectionBadge(true);
}

/**
 * Publish one SCD41 measurement to Home Assistant.
 * 将一组 SCD41 测量值发布到 Home Assistant。
 */
void publishMeasurementToHomeAssistant(uint16_t co2Ppm, float temperature,
                                       float relativeHumidity) {
    if (!homeAssistantSensorsReady) {
        return;
    }

    co2Sensor->setValue(co2Ppm);
    temperatureSensor->setValue(temperature);
    humiditySensor->setValue(relativeHumidity);
}

/**
 * Read and display a new SCD41 measurement when available.
 * 在新数据可用时读取并显示 SCD41 测量值。
 */
void updateSensorMeasurement() {
    bool dataReady = false;
    uint16_t co2Ppm = 0;
    float temperature = 0.0F;
    float relativeHumidity = 0.0F;

    int16_t error = scd41.getDataReadyStatus(dataReady);
    if (error != NO_ERROR) {
        printSensorError("data-ready check", error);
        sensorRunning = false;
        lastSensorRetryMs = millis();
        drawSensorError();
        return;
    }

    if (!dataReady) {
        return;
    }

    error = scd41.readMeasurement(co2Ppm, temperature, relativeHumidity);
    if (error != NO_ERROR) {
        printSensorError("measurement read", error);
        sensorRunning = false;
        lastSensorRetryMs = millis();
        drawSensorError();
        return;
    }

    if (co2Ppm == 0) {
        Serial.println("SCD41 measurement is not ready yet");
        drawWaitingScreen();
        return;
    }

    Serial.print("CO2: ");
    Serial.print(co2Ppm);
    Serial.print(" ppm, Temperature: ");
    Serial.print(temperature, 1);
    Serial.print(" C, Humidity: ");
    Serial.print(relativeHumidity, 1);
    Serial.println(" %");

    drawMeasurement(co2Ppm, temperature, relativeHumidity);
    publishMeasurementToHomeAssistant(co2Ppm, temperature, relativeHumidity);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("Air quality display starting");

    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, LOW);
    SPI.begin(LCD_SCK_PIN, -1, LCD_MOSI_PIN, LCD_CS_PIN);
    display.init(172, 320);
    display.setRotation(1);
    drawStaticLayout();
    drawMessageCard("AIR MONITOR", "INITIALIZING SCD41", COLOR_INFO);
    drawMetricPlaceholder(8, "TEMPERATURE");
    drawMetricPlaceholder(164, "HUMIDITY");
    digitalWrite(LCD_BL_PIN, HIGH);

    Wire.begin(SENSOR_SDA_PIN, SENSOR_SCL_PIN);
    sensorRunning = initializeSensor();
    if (sensorRunning) {
        drawWaitingScreen();
        lastSensorPollMs = millis();
    } else {
        drawSensorError();
        lastSensorRetryMs = millis();
    }

    initializeHomeAssistant();
}

void loop() {
    const unsigned long now = millis();

    ha.handle();
    updateConnectionBadge();

    if (!sensorRunning) {
        if (now - lastSensorRetryMs >= SENSOR_RETRY_INTERVAL_MS) {
            lastSensorRetryMs = now;
            Serial.println("Retrying SCD41 initialization");
            sensorRunning = initializeSensor();
            if (sensorRunning) {
                drawWaitingScreen();
                lastSensorPollMs = millis();
            }
        }
        return;
    }

    if (now - lastSensorPollMs < SENSOR_POLL_INTERVAL_MS) {
        return;
    }

    lastSensorPollMs = now;
    updateSensorMeasurement();
}
