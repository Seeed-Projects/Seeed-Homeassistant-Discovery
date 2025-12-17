/**
 * ============================================================================
 * Seeed HA Discovery - LED Switch Example
 * Seeed HA Discovery - LED 开关示例
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Create a switch entity to control LED
 *    创建一个开关实体控制 LED
 * 2. Receive switch commands from Home Assistant
 *    接收来自 Home Assistant 的开关命令
 * 3. Control LED on/off in real-time from HA interface
 *    在 HA 界面实时控制 LED 亮灭
 *
 * Important Tips:
 * 重要提示：
 * - XIAO ESP32-C3 has no User LED, external LED required
 *   XIAO ESP32-C3 没有用户 LED，需要外接 LED
 * - XIAO ESP32-S3 User LED is on GPIO21
 *   XIAO ESP32-S3 的用户 LED 在 GPIO21
 * - XIAO ESP32-C6 User LED is on GPIO15
 *   XIAO ESP32-C6 的用户 LED 在 GPIO15
 * - Modify LED_PIN according to your board
 *   如果你的开发板有板载 LED，请根据实际情况修改 LED_PIN
 *
 * External LED Wiring:
 * 外接 LED 接线方法：
 * - LED positive (long leg) -> GPIO (through 220 ohm resistor)
 *   LED 正极 (长脚) -> GPIO (通过 220Ω 电阻)
 * - LED negative (short leg) -> GND
 *   LED 负极 (短脚) -> GND
 *
 * Hardware Requirements:
 * 硬件要求：
 * - XIAO ESP32-C3/C5/C6/S3 or other ESP32 development boards
 *   XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
 * - Note: XIAO ESP32-C5 supports both 2.4GHz and 5GHz WiFi
 *   注意：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi
 * - LED + 220 ohm resistor (if external LED needed)
 *   LED + 220Ω 电阻（如果需要外接）
 *
 * Software Dependencies:
 * 软件依赖：
 * - ArduinoJson (by Benoit Blanchon)
 * - WebSockets (by Markus Sattler)
 *
 * Usage:
 * 使用方法：
 * 1. Modify WiFi configuration and LED pin below
 *    修改下方的 WiFi 配置和 LED 引脚
 * 2. Upload to ESP32
 *    上传到 ESP32
 * 3. Open Serial Monitor to view IP address
 *    打开串口监视器查看 IP 地址
 * 4. Add device in Home Assistant
 *    在 Home Assistant 中添加设备
 * 5. Control LED switch from HA interface
 *    在 HA 界面控制 LED 开关
 *
 * @author limengdu
 * @version 1.2.0
 */

#include <SeeedHADiscovery.h>

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
const char* AP_SSID = "Seeed_LED_AP";

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

// =============================================================================
// LED Pin Configuration | LED 引脚配置
// =============================================================================

// Use LED_BUILTIN macro for maximum compatibility
// Most boards define this macro pointing to onboard LED
// 使用 LED_BUILTIN 宏最大化兼容性
// 大多数开发板都定义了这个宏指向板载 LED

// Note: XIAO ESP32-C3 has no User LED, external LED required!
// If using XIAO ESP32-C3, uncomment below and connect external LED:
// 注意：XIAO ESP32-C3 没有用户 LED，需要外接！
// 如果你使用 XIAO ESP32-C3，请取消下面的注释并连接外部 LED：
// #undef LED_BUILTIN
// #define LED_BUILTIN D0  // External LED pin | 外接 LED 的引脚

// LED Polarity Configuration | LED 极性配置
// true  = Active LOW (XIAO series are active low)
// false = Active HIGH (external LEDs are usually active high)
// true  = 低电平点亮 (XIAO 系列都是低电平点亮)
// false = 高电平点亮 (外接 LED 通常是高电平点亮)
#define LED_ACTIVE_LOW true

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

SeeedHADiscovery ha;
SeeedHASwitch* ledSwitch;

// =============================================================================
// Helper Functions | 辅助函数
// =============================================================================

/**
 * Set LED state
 * 设置 LED 状态
 */
void setLED(bool on) {
    if (LED_ACTIVE_LOW) {
        digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
    } else {
        digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
    }
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
    Serial.println("  Seeed HA Discovery - LED Switch");
    Serial.println("========================================");
    Serial.println();

    // Initialize LED pin | 初始化 LED 引脚
    pinMode(LED_BUILTIN, OUTPUT);
    setLED(false);  // Initial state: OFF | 初始状态为关闭

    Serial.print("LED Pin: GPIO");
    Serial.println(LED_BUILTIN);
    Serial.print("LED Polarity: ");
    Serial.println(LED_ACTIVE_LOW ? "Active LOW" : "Active HIGH");

    // Configure device info | 配置设备信息
    ha.setDeviceInfo(
        "LED Controller",        // Device name | 设备名称
        "XIAO ESP32",           // Device model | 设备型号
        "1.0.0"                 // Firmware version | 固件版本
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
        
        // Blink LED to indicate provisioning mode | LED 闪烁指示配网模式
        for (int i = 0; i < 3; i++) {
            setLED(true);
            delay(200);
            setLED(false);
            delay(200);
        }
        return;
    }
#else
    Serial.println("Connecting to WiFi...");
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi connection failed!");
        while (1) {
            setLED(true);
            delay(200);
            setLED(false);
            delay(200);
        }
    }
#endif

    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(ha.getLocalIP().toString().c_str());

    // =========================================================================
    // Create LED switch | 创建 LED 开关
    // =========================================================================

    ledSwitch = ha.addSwitch("led", "LED", "mdi:led-on");

    // Register callback - executed when HA sends command
    // 注册回调 - 当 HA 发送命令时执行
    ledSwitch->onStateChange([](bool state) {
        Serial.print("Command received: ");
        Serial.println(state ? "ON" : "OFF");
        setLED(state);
        Serial.print("LED is now ");
        Serial.println(state ? "ON" : "OFF");
    });

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
