/**
 * ============================================================================
 * HAStateSubscribe - Receive Home Assistant Entity States
 * HAStateSubscribe - 接收 Home Assistant 实体状态
 * ============================================================================
 *
 * This example demonstrates how to receive entity states from Home Assistant.
 * 这个示例演示如何从 Home Assistant 接收实体状态。
 *
 * Use Case:
 * 使用场景：
 * - Display other sensors' values on a screen | 在屏幕上显示其他传感器的值
 * - React to changes in other HA entities | 响应其他 HA 实体的变化
 * - Create a dashboard device | 创建仪表板设备
 *
 * =============================================================================
 * TWO MODES OF OPERATION | 两种工作模式
 * =============================================================================
 *
 * Mode 1: PUSH MODE (Event-driven, recommended)
 * 模式 1：推送模式（事件驱动，推荐）
 * - HA automatically pushes state updates when entity changes
 *   HA 在实体变化时自动推送状态更新
 * - Use onHAState() callback to react immediately
 *   使用 onHAState() 回调来立即响应
 * - Best for: Real-time reactions, logging, alerts
 *   适用于：实时响应、日志记录、警报
 *
 * Mode 2: POLLING MODE (Timer-based)
 * 模式 2：轮询模式（定时器）
 * - Periodically read stored states using getHAState()
 *   使用 getHAState() 定期读取存储的状态
 * - States are still updated by HA push, you just read them on your schedule
 *   状态仍然由 HA 推送更新，你只是按自己的时间表读取
 * - Best for: Screen refresh, periodic display updates
 *   适用于：屏幕刷新、定期显示更新
 *
 * =============================================================================
 *
 * Setup Steps:
 * 设置步骤：
 * 1. Upload this sketch to your XIAO ESP32
 *    将此程序上传到你的 XIAO ESP32
 * 2. In Home Assistant, find your device and click "Configure"
 *    在 Home Assistant 中找到你的设备并点击"配置"
 * 3. Select the entities you want to subscribe
 *    选择你想要订阅的实体
 * 4. Save the configuration
 *    保存配置
 * 5. The device will automatically receive state updates
 *    设备将自动接收状态更新
 *
 * Hardware:
 * 硬件：
 * - XIAO ESP32-C3, ESP32-C5, ESP32-C6, or ESP32-S3
 * - Note: XIAO ESP32-C5 supports both 2.4GHz and 5GHz WiFi
 *   注意：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi
 *
 * @author Seeed Studio
 * @version 1.0.0
 */

#include <SeeedHADiscovery.h>

// =============================================================================
// Configuration | 配置
// =============================================================================

// WiFi Provisioning Configuration | WiFi 配网配置
// Set to true to enable web-based WiFi provisioning (recommended)
// Set to false to use hardcoded credentials below
// 设置为 true 启用网页配网（推荐）
// 设置为 false 使用下面的硬编码凭据
#define USE_WIFI_PROVISIONING true

// AP hotspot name for WiFi provisioning | 配网时的 AP 热点名称
const char* AP_SSID = "Seeed_StateDisplay_AP";

// Fallback WiFi credentials (only used if USE_WIFI_PROVISIONING is false)
// 备用 WiFi 凭据（仅在 USE_WIFI_PROVISIONING 为 false 时使用）
const char* WIFI_SSID = "your-wifi-ssid";       // Your WiFi name | 你的 WiFi 名称
const char* WIFI_PASSWORD = "your-wifi-password"; // Your WiFi password | 你的 WiFi 密码

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
// Global Variables | 全局变量
// =============================================================================

// Main HA discovery instance | 主 HA 发现实例
SeeedHADiscovery ha;

// Variables to store last printed values | 存储上次打印的值的变量
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 5000; // Print every 5 seconds | 每5秒打印一次

// =============================================================================
// Setup | 初始化
// =============================================================================

void setup() {
    // Initialize Serial | 初始化串口
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("  HA State Subscribe Example");
    Serial.println("====================================");

    // Enable debug output | 启用调试输出
    ha.enableDebug(true);

    // Set device info | 设置设备信息
    ha.setDeviceInfo(
        "HA State Display",    // Device name | 设备名称
        "XIAO ESP32",          // Device model | 设备型号
        "1.0.0"                // Firmware version | 固件版本
    );

    // Register callback for HA state changes
    // 注册 HA 状态变化回调
    // This will be called every time HA pushes a state update
    // 每次 HA 推送状态更新时都会调用此函数
    ha.onHAState([](const char* entityId, const char* state, JsonObject& attrs) {
        Serial.println();
        Serial.println("========== HA State Update ==========");
        Serial.print("Entity ID: ");
        Serial.println(entityId);
        Serial.print("State: ");
        Serial.println(state);
        
        // Print attributes | 打印属性
        if (attrs.containsKey("friendly_name")) {
            Serial.print("Friendly Name: ");
            Serial.println(attrs["friendly_name"].as<const char*>());
        }
        if (attrs.containsKey("unit_of_measurement")) {
            Serial.print("Unit: ");
            Serial.println(attrs["unit_of_measurement"].as<const char*>());
        }
        if (attrs.containsKey("device_class")) {
            Serial.print("Device Class: ");
            Serial.println(attrs["device_class"].as<const char*>());
        }
        Serial.println("=====================================");
        Serial.println();
    });

    // Set WiFi band mode for ESP32-C5 (optional)
    // 为 ESP32-C5 设置 WiFi 频段模式（可选）
    #if defined(WIFI_BAND_MODE) && defined(CONFIG_SOC_WIFI_SUPPORT_5G)
        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
            WiFi.setBandMode(WIFI_BAND_MODE);
            Serial.println("WiFi band mode configured (ESP32-C5 5GHz support)");
        #endif
    #endif

    // Connect to WiFi and start services | 连接 WiFi 并启动服务
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
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("Failed to start! Check WiFi credentials.");
        while (1) {
            delay(1000);
        }
    }
#endif

    Serial.println();
    Serial.println("====================================");
    Serial.println("  Setup Complete!");
    Serial.println("====================================");
    Serial.println();
    Serial.println("Now go to Home Assistant:");
    Serial.println("1. Find your device in Settings > Devices & Services");
    Serial.println("2. Click 'Configure' on your device");
    Serial.println("3. Select entities to subscribe");
    Serial.println("4. Save and watch the Serial output!");
    Serial.println();
}

// =============================================================================
// Main Loop | 主循环
// =============================================================================

void loop() {
    // Handle network events | 处理网络事件
    ha.handle();

#if USE_WIFI_PROVISIONING
    // If in provisioning mode, only handle AP | 如果处于配网模式，只处理 AP
    if (ha.isProvisioningActive()) {
        delay(10);
        return;
    }
#endif

    // Periodically print all subscribed states
    // 定期打印所有订阅的状态
    unsigned long now = millis();
    if (now - lastPrintTime > PRINT_INTERVAL) {
        lastPrintTime = now;
        printAllHAStates();
    }
}

// =============================================================================
// Helper Functions | 辅助函数
// =============================================================================

/**
 * Print all subscribed HA entity states
 * 打印所有订阅的 HA 实体状态
 */
void printAllHAStates() {
    const auto& states = ha.getHAStates();
    
    if (states.empty()) {
        Serial.println("[Info] No HA entities subscribed yet.");
        Serial.println("       Configure subscriptions in HA integration settings.");
        return;
    }

    Serial.println();
    Serial.println("========== All Subscribed States ==========");
    
    for (const auto& pair : states) {
        SeeedHAState* state = pair.second;
        
        if (state->hasValue()) {
            Serial.print("  ");
            Serial.print(state->getEntityId());
            Serial.print(": ");
            Serial.print(state->getString());
            
            // Add unit if available | 如果有单位则添加
            if (state->getUnit().length() > 0) {
                Serial.print(" ");
                Serial.print(state->getUnit());
            }
            
            // Show friendly name if different | 如果不同则显示友好名称
            if (state->getFriendlyName().length() > 0) {
                Serial.print(" (");
                Serial.print(state->getFriendlyName());
                Serial.print(")");
            }
            
            Serial.println();
        } else {
            Serial.print("  ");
            Serial.print(state->getEntityId());
            Serial.println(": [waiting for value]");
        }
    }
    
    Serial.println("=============================================");
    Serial.println();
}

// =============================================================================
// POLLING MODE EXAMPLES | 轮询模式示例
// =============================================================================
// 
// The following examples show how to use POLLING MODE to periodically
// read HA entity states. This is useful for screen refresh scenarios.
// 
// 以下示例展示如何使用轮询模式定期读取 HA 实体状态。
// 这对于屏幕刷新场景非常有用。
//
// Note: States are still PUSHED by HA automatically. Polling just reads
// the already-updated local cache on your own schedule.
// 注意：状态仍然由 HA 自动推送。轮询只是按你自己的时间表读取已更新的本地缓存。
// =============================================================================

/**
 * Example 1: Simple polling for specific entities
 * 示例 1：简单轮询特定实体
 *
 * Call this function periodically (e.g., every 1 second) to update display.
 * 定期调用此函数（如每 1 秒）来更新显示。
 */
/*
void updateDisplayPolling() {
    // Get a specific HA entity state
    // 获取特定的 HA 实体状态
    SeeedHAState* tempState = ha.getHAState("sensor.living_room_temperature");
    
    if (tempState && tempState->hasValue()) {
        float temperature = tempState->getFloat();
        String unit = tempState->getUnit();
        
        // Display on your screen
        // 在你的屏幕上显示
        Serial.print("Living Room: ");
        Serial.print(temperature, 1);
        Serial.println(unit);
        
        // Example: Update OLED/LCD display
        // 示例：更新 OLED/LCD 显示屏
        // display.clear();
        // display.printf("Temp: %.1f%s", temperature, unit.c_str());
        // display.display();
    }
    
    // Get humidity | 获取湿度
    SeeedHAState* humState = ha.getHAState("sensor.outdoor_humidity");
    if (humState && humState->hasValue()) {
        float humidity = humState->getFloat();
        Serial.printf("Humidity: %.1f%%\n", humidity);
    }
    
    // Get light state (boolean) | 获取灯状态（布尔值）
    SeeedHAState* lightState = ha.getHAState("light.living_room");
    if (lightState && lightState->hasValue()) {
        bool isOn = lightState->getBool();
        Serial.printf("Light: %s\n", isOn ? "ON" : "OFF");
    }
}
*/

/**
 * Example 2: Complete polling loop with timer
 * 示例 2：带定时器的完整轮询循环
 *
 * Replace the loop() function with this for a polling-based approach:
 * 用这个替换 loop() 函数来实现轮询方式：
 */
/*
// Screen refresh interval | 屏幕刷新间隔
const unsigned long SCREEN_REFRESH_INTERVAL = 1000; // 1 second | 1秒
unsigned long lastScreenRefresh = 0;

void loop() {
    // Must always call handle() | 必须始终调用 handle()
    ha.handle();
    
    // Polling: Refresh screen at fixed interval
    // 轮询：以固定间隔刷新屏幕
    unsigned long now = millis();
    if (now - lastScreenRefresh >= SCREEN_REFRESH_INTERVAL) {
        lastScreenRefresh = now;
        
        // Update screen with latest HA states
        // 使用最新的 HA 状态更新屏幕
        refreshScreen();
    }
}

void refreshScreen() {
    // Clear screen | 清屏
    // display.clear();
    
    int y = 0;
    
    // Iterate through all subscribed entities
    // 遍历所有订阅的实体
    for (const auto& pair : ha.getHAStates()) {
        SeeedHAState* state = pair.second;
        
        if (state->hasValue()) {
            String line = state->getFriendlyName();
            if (line.length() == 0) {
                line = state->getEntityId();
            }
            line += ": " + state->getString();
            if (state->getUnit().length() > 0) {
                line += " " + state->getUnit();
            }
            
            // Draw on screen | 绘制到屏幕
            // display.drawString(0, y, line);
            Serial.println(line);
            y += 16; // Line height | 行高
        }
    }
    
    // display.display();
}
*/

/**
 * Example 3: Dashboard with multiple cards
 * 示例 3：多卡片仪表板
 *
 * For displays with enough space, show multiple entity cards.
 * 对于空间足够的显示屏，显示多个实体卡片。
 */
/*
void drawDashboard() {
    // Define entity IDs you want to display
    // 定义要显示的实体 ID
    const char* entityIds[] = {
        "sensor.living_room_temperature",
        "sensor.living_room_humidity",
        "sensor.outdoor_temperature",
        "light.living_room",
        "switch.fan"
    };
    const int numEntities = 5;
    
    Serial.println("\n=== Dashboard ===");
    
    for (int i = 0; i < numEntities; i++) {
        SeeedHAState* state = ha.getHAState(entityIds[i]);
        
        Serial.print("[");
        Serial.print(i + 1);
        Serial.print("] ");
        
        if (state == nullptr) {
            // Entity not subscribed | 实体未订阅
            Serial.print(entityIds[i]);
            Serial.println(": Not subscribed");
        } else if (!state->hasValue()) {
            // Waiting for value | 等待数据
            Serial.print(state->getFriendlyName());
            Serial.println(": Waiting...");
        } else {
            // Display value | 显示值
            String name = state->getFriendlyName();
            if (name.length() == 0) name = state->getEntityId();
            
            Serial.print(name);
            Serial.print(": ");
            Serial.print(state->getString());
            
            if (state->getUnit().length() > 0) {
                Serial.print(" ");
                Serial.print(state->getUnit());
            }
            Serial.println();
        }
    }
    
    Serial.println("=================\n");
}
*/

/**
 * Example 4: Conditional logic based on HA states
 * 示例 4：基于 HA 状态的条件逻辑
 */
/*
void checkConditions() {
    // Check temperature threshold | 检查温度阈值
    SeeedHAState* temp = ha.getHAState("sensor.living_room_temperature");
    if (temp && temp->hasValue()) {
        float t = temp->getFloat();
        if (t > 28.0) {
            Serial.println("Warning: Temperature too high!");
            // Turn on fan, send alert, etc.
            // 打开风扇、发送警报等
        }
    }
    
    // Check if someone is home | 检查是否有人在家
    SeeedHAState* presence = ha.getHAState("binary_sensor.someone_home");
    if (presence && presence->hasValue()) {
        bool isHome = presence->getBool();
        if (isHome) {
            // Someone is home, show welcome message
            // 有人在家，显示欢迎消息
            Serial.println("Welcome home!");
        }
    }
}
*/
