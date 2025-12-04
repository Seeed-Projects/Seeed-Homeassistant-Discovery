/**
 * ============================================================================
 * Seeed HA Discovery BLE - 温湿度传感器示例
 * Temperature & Humidity Sensor Example (BLE)
 * ============================================================================
 *
 * 这个示例展示如何：
 * 1. 通过 BLE 广播传感器数据到 Home Assistant
 * 2. 使用 BTHome v2 协议实现自动发现
 * 3. 超低功耗运行
 *
 * ⚠️ 重要提示：
 * - 此示例适用于 XIAO ESP32-C3/C6/S3 和 XIAO nRF52840
 * - Home Assistant 需要有蓝牙适配器或 ESP32 蓝牙代理
 * - 无需安装任何 HA 插件，BTHome 设备会被自动发现
 *
 * 硬件要求：
 * - XIAO ESP32-C3/C6/S3 或 XIAO nRF52840
 * - 可选：DHT22 温湿度传感器
 *
 * 软件依赖：
 * - NimBLE-Arduino (ESP32)
 * - Adafruit Bluefruit (nRF52840, 已内置)
 *
 * @author limengdu
 * @version 1.0.0
 */

#include <SeeedHADiscoveryBLE.h>

// =============================================================================
// 配置区域
// =============================================================================

// 设备名称（会显示在 Home Assistant 中）
const char* DEVICE_NAME = "XIAO 温湿度传感器";

// 广播间隔（毫秒）- 越长越省电
const uint32_t ADVERTISE_INTERVAL = 10000;  // 10 秒

// =============================================================================
// 全局变量
// =============================================================================

SeeedHADiscoveryBLE ble;
SeeedBLESensor* tempSensor;
SeeedBLESensor* humiditySensor;
SeeedBLESensor* batterySensor;

// =============================================================================
// 辅助函数
// =============================================================================

/**
 * 读取温度（模拟数据）
 * 实际使用时替换为真实传感器读取
 */
float readTemperature() {
    static float temp = 25.0;
    temp += (random(-10, 11)) / 100.0;
    if (temp < 20) temp = 20;
    if (temp > 30) temp = 30;
    return temp;
}

/**
 * 读取湿度（模拟数据）
 */
float readHumidity() {
    static float humidity = 55.0;
    humidity += (random(-10, 11)) / 50.0;
    if (humidity < 40) humidity = 40;
    if (humidity > 70) humidity = 70;
    return humidity;
}

/**
 * 读取电池电量（模拟数据）
 */
uint8_t readBattery() {
    // 模拟电池缓慢下降
    static uint8_t battery = 100;
    if (random(0, 100) < 5) {  // 5% 概率下降
        battery = max(0, battery - 1);
    }
    return battery;
}

// =============================================================================
// Arduino 主程序
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Seeed HA Discovery BLE - 温湿度示例");
    Serial.println("========================================");
    Serial.println();

    // 启用调试
    ble.enableDebug(true);

    // 初始化 BLE
    if (!ble.begin(DEVICE_NAME)) {
        Serial.println("❌ BLE 初始化失败！");
        while (1) delay(1000);
    }

    Serial.println("✅ BLE 初始化成功！");

    // 添加传感器
    tempSensor = ble.addTemperature();
    humiditySensor = ble.addHumidity();
    batterySensor = ble.addBattery();

    Serial.println("✅ 传感器已添加");

    // =========================================================================
    // 完成初始化
    // =========================================================================

    Serial.println();
    Serial.println("========================================");
    Serial.println("  初始化完成！");
    Serial.println("========================================");
    Serial.println();
    Serial.println("📱 Home Assistant 会自动发现此设备");
    Serial.println("   确保 HA 有蓝牙适配器或蓝牙代理");
    Serial.println();
    Serial.printf("设备名称: %s\n", DEVICE_NAME);
    Serial.printf("广播间隔: %d 秒\n", ADVERTISE_INTERVAL / 1000);
    Serial.println();
}

void loop() {
    // 读取传感器
    float temp = readTemperature();
    float humidity = readHumidity();
    uint8_t battery = readBattery();

    // 更新传感器值
    tempSensor->setValue(temp);
    humiditySensor->setValue(humidity);
    batterySensor->setValue(battery);

    // 发送 BLE 广播
    ble.advertise();

    Serial.printf("📡 广播: 温度=%.2f°C, 湿度=%.1f%%, 电池=%d%%\n",
                  temp, humidity, battery);

    // 等待下次广播
    delay(ADVERTISE_INTERVAL);
}

