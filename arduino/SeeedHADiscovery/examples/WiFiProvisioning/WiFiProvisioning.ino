/**
 * ============================================================================
 * Seeed HA Discovery - WiFi Provisioning Example
 * Seeed HA Discovery - WiFi 配网示例
 * ============================================================================
 *
 * This example demonstrates the web-based WiFi provisioning feature.
 * 本示例展示网页配网功能。
 *
 * Features:
 * 功能：
 * - On first boot (no saved credentials), creates an AP hotspot
 *   首次启动（无保存凭据）时，创建 AP 热点
 * - Users can configure WiFi via captive portal in browser
 *   用户可以通过浏览器中的强制门户配置 WiFi
 * - Credentials are saved and persist across reboots
 *   凭据被保存并在重启后保持
 * - Pull-to-refresh network scanning (click Refresh button)
 *   下拉刷新网络扫描（点击刷新按钮）
 * - Captive portal functionality
 *   强制门户功能
 * - Long press reset button (6s) to re-enter provisioning mode
 *   长按重置按钮（6秒）重新进入配网模式
 *
 * Usage:
 * 使用方法：
 * 1. Upload this sketch to your ESP32
 *    将此程序上传到 ESP32
 * 2. On first boot, connect to "Seeed_HA_Device_AP" WiFi
 *    首次启动时，连接到 "Seeed_HA_Device_AP" WiFi
 * 3. Open http://192.168.4.1 in your browser
 *    在浏览器中打开 http://192.168.4.1
 * 4. Select your WiFi network and enter password
 *    选择您的 WiFi 网络并输入密码
 * 5. Device will restart and connect to configured WiFi
 *    设备将重启并连接到配置的 WiFi
 * 6. To reconfigure WiFi: Long press the reset button for 6 seconds
 *    重新配网：长按重置按钮 6 秒
 *
 * Hardware Platform:
 * 硬件平台：
 * - Any ESP32 board (ESP32, ESP32-C3, ESP32-C6, ESP32-S3, etc.)
 *   任何 ESP32 开发板
 *
 * Software Dependencies:
 * 软件依赖：
 * - SeeedHADiscovery library (includes SeeedWiFiProvisioning)
 * - ArduinoJson (by Benoit Blanchon)
 * - WebSockets (by Markus Sattler)
 *
 * @author limengdu
 * @version 1.0.0
 */

#include <SeeedHADiscovery.h>

// =============================================================================
// Configuration | 配置
// =============================================================================

// AP hotspot name when no WiFi credentials are configured
// 未配置 WiFi 凭据时的 AP 热点名称
const char* AP_SSID = "Seeed_HA_Device_AP";

// Reset button pin - Long press 6 seconds to re-enter provisioning mode
// 重置按钮引脚 - 长按 6 秒重新进入配网模式
// Set to -1 to disable reset button feature
// 设置为 -1 禁用重置按钮功能
#define RESET_BUTTON_PIN 0   // GPIO0 is BOOT button on most ESP32 boards
                             // GPIO0 是大多数 ESP32 开发板上的 BOOT 按钮

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
// Global Objects | 全局对象
// =============================================================================

SeeedHADiscovery ha;

// Sensors for demo | 演示用传感器
SeeedHASensor* uptimeSensor;

// =============================================================================
// Setup | 初始化
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("============================================");
    Serial.println("  Seeed HA Discovery - WiFi Provisioning");
    Serial.println("============================================");
    Serial.println();
    
    // Configure device info | 配置设备信息
    ha.setDeviceInfo(
        "WiFi Provisioning Demo",  // Device name | 设备名称
        "ESP32",                   // Device model | 设备型号
        "1.0.0"                    // Firmware version | 固件版本
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
    
    // Enable reset button for WiFi re-provisioning
    // 启用重置按钮用于重新配网
    // Long press for 6 seconds to clear credentials and start AP mode
    // 长按 6 秒清除凭据并启动 AP 模式
#if RESET_BUTTON_PIN >= 0
    ha.enableResetButton(RESET_BUTTON_PIN);  // true = active LOW (default)
    Serial.println("Reset button enabled on GPIO" + String(RESET_BUTTON_PIN));
    Serial.println("Long press 6s to reset WiFi credentials");
    Serial.println();
#endif
    
    // Start with WiFi provisioning | 使用 WiFi 配网启动
    // This will:
    // 这会：
    // 1. Check for saved credentials | 检查保存的凭据
    // 2. If found, try to connect | 如果有，尝试连接
    // 3. If failed or no credentials, start AP mode | 如果失败或无凭据，启动 AP 模式
    bool wifiConnected = ha.beginWithProvisioning(AP_SSID);
    
    if (!wifiConnected) {
        // Device is in AP mode for WiFi configuration
        // 设备处于 AP 模式进行 WiFi 配置
        Serial.println();
        Serial.println("============================================");
        Serial.println("  WiFi Provisioning Mode");
        Serial.println("============================================");
        Serial.println();
        Serial.println("No saved WiFi credentials found.");
        Serial.println("未找到保存的 WiFi 凭据。");
        Serial.println();
        Serial.println("To configure WiFi:");
        Serial.println("配置 WiFi：");
        Serial.println("  1. Connect to WiFi: " + String(AP_SSID));
        Serial.println("     连接到 WiFi：" + String(AP_SSID));
        Serial.println("  2. Open browser: http://192.168.4.1");
        Serial.println("     打开浏览器：http://192.168.4.1");
        Serial.println("  3. Select network and enter password");
        Serial.println("     选择网络并输入密码");
        Serial.println();
        Serial.println("Device will restart after configuration.");
        Serial.println("配置后设备将重启。");
        Serial.println();
        
        // Don't create entities in AP mode, just handle provisioning
        // AP 模式下不创建实体，只处理配网
        return;
    }
    
    // WiFi connected, create entities | WiFi 已连接，创建实体
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.println("WiFi 已连接！");
    Serial.print("IP Address: ");
    Serial.println(ha.getLocalIP().toString());
    Serial.println();
    
    // Create demo sensor | 创建演示传感器
    uptimeSensor = ha.addSensor("uptime", "Uptime", "", "s");
    uptimeSensor->setPrecision(0);
    uptimeSensor->setIcon("mdi:timer");
    
    Serial.println("============================================");
    Serial.println("  Setup Complete!");
    Serial.println("============================================");
    Serial.println();
    Serial.println("Add device in Home Assistant:");
    Serial.println("在 Home Assistant 中添加设备：");
    Serial.println("  Settings -> Devices & Services -> Add Integration");
    Serial.println("  Search 'Seeed HA Discovery'");
    Serial.print("  Enter IP: ");
    Serial.println(ha.getLocalIP().toString());
    Serial.println();
#if RESET_BUTTON_PIN >= 0
    Serial.println("To reconfigure WiFi:");
    Serial.println("重新配置 WiFi：");
    Serial.println("  Long press reset button (GPIO" + String(RESET_BUTTON_PIN) + ") for 6 seconds");
    Serial.println("  长按重置按钮（GPIO" + String(RESET_BUTTON_PIN) + "）6 秒");
    Serial.println();
#endif
}

// =============================================================================
// Loop | 主循环
// =============================================================================

void loop() {
    // Must call! Handles both HA communication and WiFi provisioning
    // 必须调用！处理 HA 通信和 WiFi 配网
    ha.handle();
    
    // If in provisioning mode, don't do anything else
    // 如果处于配网模式，不做其他事情
    if (ha.isProvisioningActive()) {
        delay(10);
        return;
    }
    
    // Update uptime sensor every 5 seconds | 每 5 秒更新运行时间传感器
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        uptimeSensor->setValue(millis() / 1000);
    }
    
    delay(10);
}

