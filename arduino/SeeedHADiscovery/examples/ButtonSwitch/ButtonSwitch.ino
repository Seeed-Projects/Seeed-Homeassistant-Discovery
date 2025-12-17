/**
 * ============================================================================
 * Seeed HA Discovery - Button Switch Example
 * Seeed HA Discovery - 按钮开关示例
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Detect three types of button presses (single, double, long press)
 *    检测物理按钮的三种按法（单击、双击、长按）
 * 2. Each press type corresponds to an independent switch state
 *    每种按法对应一个独立的开关状态
 * 3. Both physical button and Home Assistant can control switch states
 *    物理按钮和 Home Assistant 都可以控制开关状态
 * 4. Real-time state synchronization to Home Assistant
 *    实时同步状态到 Home Assistant
 *
 * Hardware Requirements:
 * 硬件要求：
 * - XIAO ESP32-C3/C5/C6/S3 or other ESP32 development boards
 *   XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
 * - Note: XIAO ESP32-C5 supports both 2.4GHz and 5GHz WiFi
 *   注意：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi
 * - Button (with internal or external pull-up resistor)
 *   按钮（内置上拉电阻或外接上拉电阻）
 *
 * Button Wiring:
 * 按钮接线方法：
 * - Button terminal 1 → GPIO (default D1)
 *   按钮一端 → GPIO (默认 D1)
 * - Button terminal 2 → GND
 *   按钮另一端 → GND
 * - Internal pull-up resistor enabled
 *   内部上拉电阻已启用
 *
 * Software Dependencies:
 * 软件依赖：
 * - ArduinoJson (by Benoit Blanchon)
 * - WebSockets (by Markus Sattler)
 *
 * Usage:
 * 使用方法：
 * 1. Modify WiFi configuration and button pin below
 *    修改下方的 WiFi 配置和按钮引脚
 * 2. Upload to ESP32
 *    上传到 ESP32
 * 3. Open Serial Monitor to view IP address
 *    打开串口监视器查看 IP 地址
 * 4. Add device in Home Assistant
 *    在 Home Assistant 中添加设备
 * 5. Try different button presses and observe switch state changes in HA
 *    尝试按钮的不同按法，观察 HA 中开关状态变化
 *
 * Button Operations:
 * 按键操作：
 * - Single click: Toggle "Single Click Switch" state
 *   单击：切换"单击开关"状态
 * - Double click: Toggle "Double Click Switch" state
 *   双击：切换"双击开关"状态
 * - Long press (>1s): Toggle "Long Press Switch" state
 *   长按 (>1秒)：切换"长按开关"状态
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
const char* AP_SSID = "Seeed_Button_AP";

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

// Button Pin | 按钮引脚
#define BUTTON_PIN D1

// Button Detection Parameters | 按钮检测参数
#define LONG_PRESS_TIME 1000      // Long press threshold (ms) | 长按阈值（毫秒）
#define DOUBLE_CLICK_TIME 300     // Double click interval (ms) | 双击间隔（毫秒）

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

SeeedHADiscovery ha;

// Three switches for three press types
// 三个开关，对应三种按法
SeeedHASwitch* singleClickSwitch;
SeeedHASwitch* doubleClickSwitch;
SeeedHASwitch* longPressSwitch;

// Button state | 按钮状态
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
unsigned long lastClickTime = 0;
uint8_t clickCount = 0;

// =============================================================================
// Helper Functions | 辅助函数
// =============================================================================

/**
 * Detect button event
 * 检测按钮事件
 */
enum ButtonEvent {
    BUTTON_NONE,
    BUTTON_SINGLE,
    BUTTON_DOUBLE,
    BUTTON_LONG
};

ButtonEvent detectButtonEvent() {
    bool currentState = digitalRead(BUTTON_PIN);
    ButtonEvent event = BUTTON_NONE;
    unsigned long now = millis();

    // Detect press | 检测按下
    if (lastButtonState == HIGH && currentState == LOW) {
        buttonPressTime = now;
    }

    // Detect release | 检测释放
    if (lastButtonState == LOW && currentState == HIGH) {
        unsigned long pressDuration = now - buttonPressTime;

        if (pressDuration >= LONG_PRESS_TIME) {
            // Long press | 长按
            event = BUTTON_LONG;
            clickCount = 0;
        } else {
            // Short press, detect double click
            // 短按，检测双击
            if (now - lastClickTime < DOUBLE_CLICK_TIME) {
                clickCount++;
            } else {
                clickCount = 1;
            }
            lastClickTime = now;
        }
    }

    // Detect double click timeout | 检测双击超时
    if (clickCount > 0 && now - lastClickTime > DOUBLE_CLICK_TIME) {
        if (clickCount == 1) {
            event = BUTTON_SINGLE;
        } else {
            event = BUTTON_DOUBLE;
        }
        clickCount = 0;
    }

    lastButtonState = currentState;
    return event;
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
    Serial.println("  Seeed HA Discovery - Button Switch");
    Serial.println("========================================");
    Serial.println();

    // Initialize button pin | 初始化按钮引脚
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.print("Button Pin: D1 (GPIO");
    Serial.print(BUTTON_PIN);
    Serial.println(")");

    // Configure device info | 配置设备信息
    ha.setDeviceInfo(
        "Button Controller",     // Device name | 设备名称
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
        
        // In provisioning mode, loop handles AP
        // 在配网模式下，loop 处理 AP
        return;
    }
#else
    Serial.println("Connecting to WiFi...");
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi connection failed!");
        while (1) {
            delay(1000);
        }
    }
#endif

    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(ha.getLocalIP().toString().c_str());

    // =========================================================================
    // Create three switches for three press types
    // 创建三个开关，对应三种按法
    // =========================================================================

    singleClickSwitch = ha.addSwitch("single", "Single Click", "mdi:gesture-tap");
    doubleClickSwitch = ha.addSwitch("double", "Double Click", "mdi:gesture-double-tap");
    longPressSwitch = ha.addSwitch("long", "Long Press", "mdi:gesture-tap-hold");

    // Register callbacks - executed when HA sends commands
    // 注册回调 - 当 HA 发送命令时执行
    singleClickSwitch->onStateChange([](bool state) {
        Serial.print("HA Control [Single Click]: ");
        Serial.println(state ? "ON" : "OFF");
    });

    doubleClickSwitch->onStateChange([](bool state) {
        Serial.print("HA Control [Double Click]: ");
        Serial.println(state ? "ON" : "OFF");
    });

    longPressSwitch->onStateChange([](bool state) {
        Serial.print("HA Control [Long Press]: ");
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
    Serial.println("Supported button operations:");
    Serial.println("  - Single click: Toggle 'Single Click' switch");
    Serial.println("  - Double click: Toggle 'Double Click' switch");
    Serial.println("  - Long press (>1s): Toggle 'Long Press' switch");
    Serial.println();
    Serial.println("Waiting for button events...");
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

    // Detect button event | 检测按钮事件
    ButtonEvent event = detectButtonEvent();

    // If event detected, toggle corresponding switch state
    // 如果有事件，切换对应开关的状态
    if (event != BUTTON_NONE) {
        const char* eventName = "Unknown";
        SeeedHASwitch* targetSwitch = nullptr;

        switch (event) {
            case BUTTON_SINGLE:
                eventName = "Single Click";
                targetSwitch = singleClickSwitch;
                break;
            case BUTTON_DOUBLE:
                eventName = "Double Click";
                targetSwitch = doubleClickSwitch;
                break;
            case BUTTON_LONG:
                eventName = "Long Press";
                targetSwitch = longPressSwitch;
                break;
            default:
                break;
        }

        if (targetSwitch) {
            // Toggle state | 切换状态
            bool newState = !targetSwitch->getState();

            Serial.print("Button Event: ");
            Serial.print(eventName);
            Serial.print(" -> Switch State: ");
            Serial.println(newState ? "ON" : "OFF");

            // Update switch state (sync to HA)
            // 更新开关状态（同步到 HA）
            targetSwitch->setState(newState);
        }
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
