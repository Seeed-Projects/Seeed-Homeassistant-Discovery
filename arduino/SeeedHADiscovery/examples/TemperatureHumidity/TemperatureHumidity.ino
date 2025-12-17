/**
 * ============================================================================
 * Seeed HA Discovery - Temperature & Humidity Sensor Example
 * Seeed HA Discovery - 温湿度传感器示例
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Connect ESP32 to WiFi
 *    将 ESP32 连接到 WiFi
 * 2. Create temperature and humidity sensors
 *    创建温度和湿度传感器
 * 3. Report sensor data to Home Assistant in real-time
 *    向 Home Assistant 实时上报传感器数据
 *
 * Hardware Requirements:
 * 硬件要求：
 * - XIAO ESP32-C3/C5/C6/S3 or other ESP32 development boards
 *   XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
 * - Note: XIAO ESP32-C5 supports both 2.4GHz and 5GHz WiFi
 *   注意：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi
 * - DHT22 temperature/humidity sensor (optional, can use simulated data)
 *   DHT22 温湿度传感器（可选，本示例也可使用模拟数据）
 *
 * DHT22 Wiring:
 * DHT22 接线说明：
 * - VCC  -> 3.3V
 * - GND  -> GND
 * - DATA -> D2 (configurable below)
 *   DATA -> D2 (可在下方修改)
 *
 * Software Dependencies:
 * 软件依赖：
 * - ArduinoJson (by Benoit Blanchon)
 * - WebSockets (by Markus Sattler)
 * - DHT sensor library (by Adafruit) - if using DHT22
 *   DHT sensor library (作者: Adafruit) - 如果使用 DHT22
 *
 * Usage:
 * 使用方法：
 * 1. Modify WiFi configuration below
 *    修改下方的 WiFi 配置
 * 2. If using DHT22, uncomment USE_DHT_SENSOR
 *    如果使用 DHT22，取消注释 USE_DHT_SENSOR
 * 3. Upload to ESP32
 *    上传到 ESP32
 * 4. Open Serial Monitor to view IP address
 *    打开串口监视器查看 IP 地址
 * 5. Add device in Home Assistant
 *    在 Home Assistant 中添加设备
 *
 * @author limengdu
 * @version 1.2.0
 */

#include <SeeedHADiscovery.h>

// If using DHT22 sensor, uncomment the following two lines
// 如果使用 DHT22 传感器，取消下面两行注释
// #include <DHT.h>
// #define USE_DHT_SENSOR

// =============================================================================
// Configuration - Please modify according to your environment
// 配置区域 - 请根据你的环境修改
// =============================================================================

// WiFi Provisioning Configuration | WiFi 配网配置
// Set to true to enable web-based WiFi provisioning (recommended)
// Set to false to use hardcoded credentials below
// 设置为 true 启用网页配网（推荐）
// 设置为 false 使用下面的硬编码凭据
#define USE_WIFI_PROVISIONING true

// AP hotspot name for WiFi provisioning | 配网时的 AP 热点名称
const char* AP_SSID = "Seeed_TempHum_AP";

// Fallback WiFi credentials (only used if USE_WIFI_PROVISIONING is false)
// 备用 WiFi 凭据（仅在 USE_WIFI_PROVISIONING 为 false 时使用）
const char* WIFI_SSID = "Your_WiFi_SSID";      // Your WiFi SSID | 你的WiFi名称
const char* WIFI_PASSWORD = "Your_WiFi_Password";  // Your WiFi password | 你的WiFi密码

// =============================================================================
// WiFi Band Mode Configuration (ESP32-C5 only) | WiFi 频段配置（仅 ESP32-C5）
// =============================================================================
// ESP32-C5 supports 5GHz WiFi. You can force a specific band mode.
// ESP32-C5 支持 5GHz WiFi，你可以强制指定频段模式。
// Requires Arduino ESP32 Core 3.3.0+ (ESP-IDF 5.4.2+)
// 需要 Arduino ESP32 Core 3.3.0+ (ESP-IDF 5.4.2+)
//
// Available modes | 可用模式:
// - WIFI_BAND_MODE_AUTO   : Auto select (default) | 自动选择（默认）
// - WIFI_BAND_MODE_2G_ONLY: 2.4GHz only | 仅 2.4GHz
// - WIFI_BAND_MODE_5G_ONLY: 5GHz only (C5 only) | 仅 5GHz（仅 C5）
//
// Uncomment to enable band mode selection | 取消注释以启用频段选择:
// #define WIFI_BAND_MODE WIFI_BAND_MODE_AUTO

// DHT Sensor Configuration (if using)
// DHT 传感器配置（如果使用）
#ifdef USE_DHT_SENSOR
    #define DHT_PIN D2        // DHT data pin | DHT 数据引脚
    #define DHT_TYPE DHT22    // DHT type: DHT11 or DHT22 | DHT 类型
    DHT dht(DHT_PIN, DHT_TYPE);
#endif

// Data reporting interval (ms) | 数据上报间隔（毫秒）
const unsigned long UPDATE_INTERVAL = 5000;  // 5 seconds | 5 秒

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

SeeedHADiscovery ha;
SeeedHASensor* tempSensor;
SeeedHASensor* humiditySensor;

// =============================================================================
// Helper Functions | 辅助函数
// =============================================================================

/**
 * Read temperature value
 * 读取温度值
 */
float readTemperature() {
    #ifdef USE_DHT_SENSOR
        float temp = dht.readTemperature();
        if (isnan(temp)) {
            Serial.println("DHT22 temperature read failed!");
            return 0;
        }
        return temp;
    #else
        // Simulated data: fluctuates between 20-30°C
        // 模拟数据：20-30°C 之间波动
        static float baseTemp = 25.0;
        baseTemp += (random(-10, 11)) / 100.0;
        if (baseTemp < 20) baseTemp = 20;
        if (baseTemp > 30) baseTemp = 30;
        return baseTemp;
    #endif
}

/**
 * Read humidity value
 * 读取湿度值
 */
float readHumidity() {
    #ifdef USE_DHT_SENSOR
        float humidity = dht.readHumidity();
        if (isnan(humidity)) {
            Serial.println("DHT22 humidity read failed!");
            return 0;
        }
        return humidity;
    #else
        // Simulated data: fluctuates between 40-70%
        // 模拟数据：40-70% 之间波动
        static float baseHumidity = 55.0;
        baseHumidity += (random(-10, 11)) / 50.0;
        if (baseHumidity < 40) baseHumidity = 40;
        if (baseHumidity > 70) baseHumidity = 70;
        return baseHumidity;
    #endif
}

// =============================================================================
// Arduino Main Program | Arduino 主程序
// =============================================================================

void setup() {
    // Initialize serial | 初始化串口
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Seeed HA Discovery - Temp/Humidity");
    Serial.println("========================================");
    Serial.println();

    // Initialize DHT sensor (if using)
    // 初始化 DHT 传感器（如果使用）
    #ifdef USE_DHT_SENSOR
        Serial.println("Initializing DHT22 sensor...");
        dht.begin();
        Serial.print("DHT22 Pin: D2 (GPIO");
        Serial.print(DHT_PIN);
        Serial.println(")");
    #else
        Serial.println("Using simulated sensor data");
        Serial.println("To use DHT22, uncomment USE_DHT_SENSOR");
    #endif
    Serial.println();

    // Configure device info | 配置设备信息
    ha.setDeviceInfo(
        "Temp/Humidity Sensor", // Device name | 设备名称
        "XIAO ESP32",          // Device model | 设备型号
        "1.0.0"                // Firmware version | 固件版本
    );

    ha.enableDebug(true);

    // Set WiFi band mode for ESP32-C5 (optional)
    // 为 ESP32-C5 设置 WiFi 频段模式（可选）
    #if defined(WIFI_BAND_MODE) && defined(CONFIG_SOC_WIFI_SUPPORT_5G)
        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
            WiFi.setBandMode(WIFI_BAND_MODE);
            Serial.println("WiFi band mode configured (ESP32-C5 5GHz support)");
        #endif
    #endif

    // Connect WiFi | 连接 WiFi
#if USE_WIFI_PROVISIONING
    Serial.println("Starting with WiFi provisioning...");
    Serial.print("  AP Name (if needed): ");
    Serial.println(AP_SSID);
    
    bool wifiConnected = ha.beginWithProvisioning(AP_SSID);
    
    if (!wifiConnected) {
        Serial.println();
        Serial.println("============================================");
        Serial.println("  WiFi Provisioning Mode Active!");
        Serial.println("============================================");
        Serial.println("  1. Connect to WiFi: " + String(AP_SSID));
        Serial.println("  2. Open browser: http://192.168.4.1");
        Serial.println("  3. Select network and enter password");
        Serial.println();
        return;
    }
#else
    Serial.println("Connecting to WiFi...");
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi connection failed!");
        while (1) delay(1000);
    }
#endif

    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(ha.getLocalIP().toString().c_str());

    // =========================================================================
    // Create sensors | 创建传感器
    // =========================================================================

    // Temperature sensor | 温度传感器
    tempSensor = ha.addSensor("temperature", "Temperature", "temperature", "°C");
    tempSensor->setPrecision(1);  // 1 decimal place | 1 位小数

    // Humidity sensor | 湿度传感器
    humiditySensor = ha.addSensor("humidity", "Humidity", "humidity", "%");
    humiditySensor->setPrecision(0);  // Integer | 整数

    // =========================================================================
    // Initialization complete | 完成初始化
    // =========================================================================

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Initialization Complete!");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Add device in Home Assistant:");
    Serial.println("  Settings -> Devices & Services -> Add Integration");
    Serial.println("  Search 'Seeed HA Discovery'");
    Serial.print("  Enter IP: ");
    Serial.println(ha.getLocalIP().toString().c_str());
    Serial.println();
    Serial.print("Device status page: http://");
    Serial.println(ha.getLocalIP().toString().c_str());
    Serial.println();
}

void loop() {
    // Must call! Handle network events
    // 必须调用！处理网络事件
    ha.handle();

#if USE_WIFI_PROVISIONING
    // If in provisioning mode, only handle AP | 如果处于配网模式，只处理 AP
    if (ha.isProvisioningActive()) {
        delay(10);
        return;
    }
#endif

    // Periodically read and report sensor data
    // 定期读取并上报传感器数据
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > UPDATE_INTERVAL) {
        lastUpdate = millis();

        float temp = readTemperature();
        float humidity = readHumidity();

        // Update sensor values (automatically push to HA)
        // 更新传感器值（自动推送到 HA）
        tempSensor->setValue(temp);
        humiditySensor->setValue(humidity);

        Serial.print("Broadcast: Temp=");
        Serial.print(temp, 1);
        Serial.print("C, Humidity=");
        Serial.print(humidity, 0);
        Serial.println("%");
    }

    // Connection status monitoring | 连接状态监控
    static unsigned long lastCheck = 0;
    static bool wasConnected = false;

    if (millis() - lastCheck > 5000) {
        lastCheck = millis();

        bool connected = ha.isHAConnected();
        if (connected != wasConnected) {
            Serial.println(connected ? "HA Connected" : "HA Disconnected");
            wasConnected = connected;
        }
    }
}
