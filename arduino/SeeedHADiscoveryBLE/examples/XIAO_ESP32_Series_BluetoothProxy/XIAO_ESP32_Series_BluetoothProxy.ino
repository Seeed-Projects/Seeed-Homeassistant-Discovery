/**
 * ============================================================================
 * Seeed HA Discovery BLE - BLE-MQTT Gateway Example
 * Seeed HA Discovery BLE - BLE-MQTT 网关示例
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Scan BTHome BLE advertisements and forward to MQTT server
 *    扫描 BTHome BLE 广播并转发到 MQTT 服务器
 * 2. Use WiFiManager for WiFi and MQTT configuration
 *    使用 WiFiManager 进行 WiFi 和 MQTT 配置
 * 3. Create a bridge between BLE devices and Home Assistant via MQTT
 *    通过 MQTT 在 BLE 设备和 Home Assistant 之间创建桥接
 *
 * Supported Boards:
 * 支持的开发板：
 * - XIAO ESP32-C3
 * - XIAO ESP32-S3
 * - XIAO ESP32-C6
 * - XIAO ESP32-C5
 *
 * Software Dependencies:
 * 软件依赖：
 * - WiFiManager (by tzapu) - WiFi captive portal configuration
 *   WiFiManager（作者：tzapu）- WiFi 强制门户配置
 * - PubSubClient (by Nick O'Leary) - MQTT client
 *   PubSubClient（作者：Nick O'Leary）- MQTT 客户端
 * - ArduinoJson (by Benoit Blanchon) - JSON serialization
 *   ArduinoJson（作者：Benoit Blanchon）- JSON 序列化
 *
 * Board Selection in Arduino IDE:
 * Arduino IDE 中的开发板选择：
 * Tools -> Board -> ESP32 Arduino -> Select your XIAO board
 * 工具 -> 开发板 -> ESP32 Arduino -> 选择你的 XIAO 开发板
 *
 * Partition Scheme:
 * 分区方案：
 * Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
 *
 * Usage:
 * 使用方法：
 * 1. First boot: Connect to "SeeedUA-Gateway" WiFi hotspot
 *    首次启动：连接到 "SeeedUA-Gateway" WiFi 热点
 * 2. Configure WiFi and MQTT settings via captive portal
 *    通过强制门户配置 WiFi 和 MQTT 设置
 * 3. Device will scan for BTHome devices and forward to MQTT
 *    设备将扫描 BTHome 设备并转发到 MQTT
 *
 * To reconfigure: Hold BOOT button within 5 seconds of power-on
 * 重新配置：开机后 5 秒内按住 BOOT 按钮
 *
 * @author limengdu
 * @version 1.1.0
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "esp_wifi.h"

// NimBLE ESP-IDF APIs
// NimBLE ESP-IDF 接口
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

// =============================================================================
// Board Auto-Detection | 开发板自动检测
// =============================================================================

// Detect board type and set appropriate pins
// 检测开发板类型并设置相应引脚
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    // XIAO ESP32-C3
    #define BOARD_NAME      "XIAO ESP32-C3"
    #define LED_PIN         10      // D10 user LED | D10 用户 LED
    #define BOOT_BUTTON_PIN 9       // BOOT button | BOOT 按钮
    #define LED_ACTIVE_LOW  true    // LED active low | LED 低电平点亮

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // XIAO ESP32-S3
    #define BOARD_NAME      "XIAO ESP32-S3"
    #define LED_PIN         21      // D21 user LED | D21 用户 LED
    #define BOOT_BUTTON_PIN 0       // BOOT button (GPIO0) | BOOT 按钮 (GPIO0)
    #define LED_ACTIVE_LOW  true    // LED active low | LED 低电平点亮

#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    // XIAO ESP32-C6
    #define BOARD_NAME      "XIAO ESP32-C6"
    #define LED_PIN         15      // D15 built-in LED | D15 内置 LED
    #define BOOT_BUTTON_PIN 9       // BOOT button | BOOT 按钮
    #define LED_ACTIVE_LOW  true    // LED active low | LED 低电平点亮

#elif defined(CONFIG_IDF_TARGET_ESP32C5)
    // XIAO ESP32-C5
    #define BOARD_NAME      "XIAO ESP32-C5"
    #define LED_PIN         27      // Built-in LED | 内置 LED
    #define BOOT_BUTTON_PIN 9       // BOOT button | BOOT 按钮
    #define LED_ACTIVE_LOW  true    // LED active low | LED 低电平点亮

#else
    // Default/Unknown board - use common defaults
    // 默认/未知开发板 - 使用通用默认值
    #define BOARD_NAME      "ESP32 (Unknown)"
    #define LED_PIN         LED_BUILTIN
    #define BOOT_BUTTON_PIN 0
    #define LED_ACTIVE_LOW  true
    #warning "Unknown board type, using default pin configuration"
#endif

// =============================================================================
// Configuration | 配置
// =============================================================================

// Gateway device name (used for WiFi AP and identification)
// 网关设备名称（用于 WiFi AP 和识别）
#define GATEWAY_NAME "SeeedUA-Gateway"

// BTHome protocol constants
// BTHome 协议常量
#define BTHOME_SERVICE_UUID_L 0xD2
#define BTHOME_SERVICE_UUID_H 0xFC
#define BTHOME_BINARY_MOTION  0x21

// =============================================================================
// BLE Scan Parameters | BLE 扫描参数
// =============================================================================

static struct ble_gap_disc_params scan_params = {
    .itvl = 0x50,              // 50ms scan interval | 50ms 扫描间隔
    .window = 0x30,            // 30ms scan window | 30ms 扫描窗口
    .filter_policy = 0,
    .limited = 0,
    .passive = 0,
    .filter_duplicates = 0
};

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;

// MQTT configuration buffers
// MQTT 配置缓冲区
char mqtt_server[64] = "";
char mqtt_port[6] = "1883";
char mqtt_user[32] = "";
char mqtt_password[64] = "";

// Configuration save flag
// 配置保存标志
bool shouldSaveConfig = false;

// LED state management
// LED 状态管理
unsigned long lastLedToggle = 0;
bool ledState = false;
enum LedMode { LED_CONFIG, LED_CONNECTING, LED_NORMAL };
LedMode currentLedMode = LED_CONNECTING;

// BLE initialization flag
// BLE 初始化标志
bool bleInitialized = false;

// =============================================================================
// Function Declarations | 函数声明
// =============================================================================

void publishToMQTT(const char* name, const char* mac, int rssi, bool motion);

// =============================================================================
// WiFiManager Callbacks | WiFiManager 回调函数
// =============================================================================

/**
 * Called when configuration has been modified
 * 当配置被修改时调用
 */
void saveConfigCallback() {
    Serial.println("Configuration modified, needs saving");
    shouldSaveConfig = true;
}

/**
 * Save MQTT configuration to flash
 * 保存 MQTT 配置到 Flash
 */
void saveConfig() {
    preferences.begin("mqtt", false);
    preferences.putString("server", mqtt_server);
    preferences.putString("port", mqtt_port);
    preferences.putString("user", mqtt_user);
    preferences.putString("password", mqtt_password);
    preferences.end();
    Serial.println("MQTT configuration saved");
}

/**
 * Load MQTT configuration from flash
 * 从 Flash 加载 MQTT 配置
 */
void loadConfig() {
    preferences.begin("mqtt", true);
    String server = preferences.getString("server", "");
    String port = preferences.getString("port", "1883");
    String user = preferences.getString("user", "");
    String password = preferences.getString("password", "");
    preferences.end();
    
    server.toCharArray(mqtt_server, sizeof(mqtt_server));
    port.toCharArray(mqtt_port, sizeof(mqtt_port));
    user.toCharArray(mqtt_user, sizeof(mqtt_user));
    password.toCharArray(mqtt_password, sizeof(mqtt_password));
    
    Serial.print("Loaded MQTT config: ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.println(mqtt_port);
}

// =============================================================================
// BLE Scan Callback | BLE 扫描回调
// =============================================================================

/**
 * BLE GAP event handler - processes discovered advertisements
 * BLE GAP 事件处理程序 - 处理发现的广播
 */
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            // Parse advertising data
            // 解析广播数据
            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc != 0) {
                return 0;
            }
            
            // Check device name
            // 检查设备名称
            if (!fields.name_is_complete || fields.name_len < 8) {
                return 0;
            }
            
            char deviceName[32] = {0};
            int nameLen = fields.name_len < 31 ? fields.name_len : 31;
            memcpy(deviceName, fields.name, nameLen);
            deviceName[nameLen] = '\0';
            
            // Filter for SeeedUA- devices only
            // 仅过滤 SeeedUA- 设备
            if (strncmp(deviceName, "SeeedUA-", 8) != 0) {
                return 0;
            }
            
            // Find BTHome Service Data (UUID 0xFCD2)
            // 查找 BTHome 服务数据（UUID 0xFCD2）
            bool foundBTHome = false;
            bool motionDetected = false;
            
            // Manually parse advertising data for Service Data
            // 手动解析广播数据以查找服务数据
            const uint8_t *data = event->disc.data;
            uint8_t len = event->disc.length_data;
            uint8_t pos = 0;
            
            while (pos < len) {
                uint8_t fieldLen = data[pos];
                if (fieldLen == 0 || pos + fieldLen >= len) break;
                
                uint8_t fieldType = data[pos + 1];
                
                // Service Data - 16 bit UUID (0x16)
                // 服务数据 - 16 位 UUID (0x16)
                if (fieldType == 0x16 && fieldLen >= 4) {
                    // Check UUID (little-endian)
                    // 检查 UUID（小端序）
                    if (data[pos + 2] == BTHOME_SERVICE_UUID_L && 
                        data[pos + 3] == BTHOME_SERVICE_UUID_H) {
                        foundBTHome = true;
                        
                        // Parse BTHome data: [DeviceInfo][ObjectID][Value]...
                        // 解析 BTHome 数据：[设备信息][对象ID][值]...
                        for (int i = pos + 5; i < pos + fieldLen; i++) {
                            if (data[i] == BTHOME_BINARY_MOTION && i + 1 < pos + fieldLen + 1) {
                                motionDetected = (data[i + 1] != 0);
                                break;
                            }
                        }
                    }
                }
                
                pos += fieldLen + 1;
            }
            
            if (!foundBTHome) {
                return 0;
            }
            
            // Format MAC address
            // 格式化 MAC 地址
            char macStr[18];
            const uint8_t *addr = event->disc.addr.val;
            sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x",
                    addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
            
            int rssi = event->disc.rssi;
            
            Serial.printf("[BLE] %s (%s) RSSI:%d Motion:%s\n",
                          deviceName, macStr, rssi,
                          motionDetected ? "Detected" : "Clear");
            
            // Publish to MQTT
            // 发布到 MQTT
            publishToMQTT(deviceName, macStr, rssi, motionDetected);
            
            // LED flash to indicate received data
            // LED 闪烁表示收到数据
            ledOff();
            delay(30);
            ledOn();
            
            break;
        }
        
        case BLE_GAP_EVENT_DISC_COMPLETE:
            // Scan complete, restart scanning
            // 扫描完成，重新开始扫描
            ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &scan_params, ble_gap_event, NULL);
            break;
            
        default:
            break;
    }
    return 0;
}

/**
 * NimBLE host task
 * NimBLE 主机任务
 */
static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * Called when BLE stack is synchronized
 * 当 BLE 协议栈同步完成时调用
 */
static void ble_on_sync(void) {
    Serial.println("[BLE] Starting scan...");
    ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &scan_params, ble_gap_event, NULL);
}

/**
 * Called when BLE stack is reset
 * 当 BLE 协议栈复位时调用
 */
static void ble_on_reset(int reason) {
    Serial.printf("[BLE] Reset, reason=%d\n", reason);
}

// =============================================================================
// MQTT Functions | MQTT 函数
// =============================================================================

/**
 * Publish BTHome device state to MQTT
 * 发布 BTHome 设备状态到 MQTT
 * 
 * @param name Device name | 设备名称
 * @param mac MAC address | MAC 地址
 * @param rssi Signal strength | 信号强度
 * @param motion Motion detected | 是否检测到运动
 */
void publishToMQTT(const char* name, const char* mac, int rssi, bool motion) {
    if (!mqttClient.connected()) {
        return;
    }
    
    // Clean MAC address (remove colons, lowercase)
    // 清理 MAC 地址（移除冒号，转小写）
    String macClean = String(mac);
    macClean.replace(":", "");
    macClean.toLowerCase();
    
    String topic = "bthome/" + macClean + "/state";
    
    // Build JSON payload
    // 构建 JSON 载荷
    JsonDocument doc;
    doc["motion"] = motion;
    doc["rssi"] = rssi;
    doc["name"] = name;
    
    String payload;
    serializeJson(doc, payload);
    
    if (mqttClient.publish(topic.c_str(), payload.c_str())) {
        Serial.printf("[MQTT] -> %s: %s\n", topic.c_str(), payload.c_str());
    } else {
        Serial.println("[MQTT] Publish failed!");
    }
}

/**
 * Connect to MQTT server
 * 连接到 MQTT 服务器
 */
void connectMQTT() {
    if (strlen(mqtt_server) == 0) {
        Serial.println("[MQTT] Server not configured");
        return;
    }
    
    Serial.print("[MQTT] Connecting to ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.println(mqtt_port);
    
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
    
    // Generate unique client ID
    // 生成唯一客户端 ID
    String clientId = "SeeedUA-GW-";
    clientId += String(random(0xffff), HEX);
    
    bool connected = false;
    if (strlen(mqtt_user) > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("[MQTT] Connected!");
        currentLedMode = LED_NORMAL;
    } else {
        Serial.print("[MQTT] Connection failed, rc=");
        Serial.println(mqttClient.state());
    }
}

// =============================================================================
// BLE Initialization | BLE 初始化
// =============================================================================

/**
 * Initialize NimBLE stack
 * 初始化 NimBLE 协议栈
 */
void initBLE() {
    Serial.println("[BLE] Initializing NimBLE...");
    
    // Initialize NimBLE port
    // 初始化 NimBLE 端口
    nimble_port_init();
    
    // Configure BLE Host callbacks
    // 配置 BLE 主机回调
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    
    // Start NimBLE FreeRTOS task
    // 启动 NimBLE FreeRTOS 任务
    nimble_port_freertos_init(ble_host_task);
    
    bleInitialized = true;
    Serial.println("[BLE] NimBLE initialization complete");
}

// =============================================================================
// LED Control | LED 控制
// =============================================================================

/**
 * Turn LED on (handles active-low/high logic)
 * 打开 LED（处理低电平/高电平点亮逻辑）
 */
void ledOn() {
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? LOW : HIGH);
}

/**
 * Turn LED off (handles active-low/high logic)
 * 关闭 LED（处理低电平/高电平点亮逻辑）
 */
void ledOff() {
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);
}

/**
 * Update LED based on current mode
 * 根据当前模式更新 LED
 */
void updateLed() {
    unsigned long now = millis();
    
    switch (currentLedMode) {
        case LED_CONFIG:
            // Fast blink for configuration mode
            // 配置模式快闪
            if (now - lastLedToggle > 200) {
                ledState = !ledState;
                if (ledState) ledOn(); else ledOff();
                lastLedToggle = now;
            }
            break;
            
        case LED_CONNECTING:
            // Slow blink for connecting
            // 连接中慢闪
            if (now - lastLedToggle > 1000) {
                ledState = !ledState;
                if (ledState) ledOn(); else ledOff();
                lastLedToggle = now;
            }
            break;
            
        case LED_NORMAL:
            // Solid on for normal operation
            // 正常运行常亮
            ledOn();
            break;
    }
}

// =============================================================================
// Arduino Main Program | Arduino 主程序
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("  XIAO ESP32 BLE-MQTT Gateway");
    Serial.println("========================================");
    Serial.println();
    Serial.print("Board: ");
    Serial.println(BOARD_NAME);
    Serial.print("LED Pin: GPIO");
    Serial.println(LED_PIN);
    Serial.print("BOOT Button: GPIO");
    Serial.println(BOOT_BUTTON_PIN);
    Serial.println();
    
    // Initialize LED
    // 初始化 LED
    pinMode(LED_PIN, OUTPUT);
    ledOff();  // Turn LED off initially | 初始关闭 LED
    
    // Load saved configuration
    // 加载保存的配置
    loadConfig();
    
    // =========================================================================
    // WiFi Initialization | WiFi 初始化
    // =========================================================================
    
    WiFi.mode(WIFI_STA);
    delay(100);
    
    // Configure WiFiManager
    // 配置 WiFiManager
    WiFiManager wm;
    wm.setSaveConfigCallback(saveConfigCallback);
    
    // Add custom MQTT parameters
    // 添加自定义 MQTT 参数
    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 64);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
    WiFiManagerParameter custom_mqtt_user("user", "MQTT User (optional)", mqtt_user, 32);
    WiFiManagerParameter custom_mqtt_password("password", "MQTT Password (optional)", mqtt_password, 64);
    
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_mqtt_user);
    wm.addParameter(&custom_mqtt_password);
    
    // Set portal timeout
    // 设置门户超时
    wm.setConfigPortalTimeout(180);
    
    currentLedMode = LED_CONFIG;
    
    Serial.println("[WiFi] Hold BOOT button within 5s to reconfigure");
    
    // Check for configuration button press
    // 检查配置按钮是否按下
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);  // BOOT button | BOOT 按钮
    delay(100);
    
    bool forceConfig = false;
    unsigned long startCheck = millis();
    while (millis() - startCheck < 5000) {
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            Serial.println("[WiFi] BOOT button detected, entering config mode...");
            forceConfig = true;
            break;
        }
        updateLed();
        delay(100);
    }
    
    currentLedMode = LED_CONNECTING;
    
    // Start WiFi connection or config portal
    // 启动 WiFi 连接或配置门户
    bool connected = false;
    if (forceConfig) {
        currentLedMode = LED_CONFIG;
        connected = wm.startConfigPortal(GATEWAY_NAME);
    } else {
        connected = wm.autoConnect(GATEWAY_NAME);
    }
    
    if (!connected) {
        Serial.println("[WiFi] Connection failed, restarting...");
        delay(3000);
        ESP.restart();
    }
    
    Serial.println("[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    
    // Copy parameters from WiFiManager
    // 从 WiFiManager 复制参数
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_password, custom_mqtt_password.getValue());
    
    // Save configuration if modified
    // 如果修改则保存配置
    if (shouldSaveConfig) {
        saveConfig();
    }
    
    // =========================================================================
    // MQTT and BLE Initialization | MQTT 和 BLE 初始化
    // =========================================================================
    
    connectMQTT();
    initBLE();
    
    // =========================================================================
    // Initialization Complete | 初始化完成
    // =========================================================================
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Initialization Complete!");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Gateway ready, scanning for BTHome devices...");
    Serial.println();
    Serial.println("MQTT Topic Format: bthome/{mac}/state");
    Serial.println("Payload: {\"motion\":true/false,\"rssi\":-XX,\"name\":\"...\"}");
    Serial.println();
}

void loop() {
    // Update LED status
    // 更新 LED 状态
    updateLed();
    
    // Handle MQTT reconnection
    // 处理 MQTT 重连
    if (!mqttClient.connected()) {
        currentLedMode = LED_CONNECTING;
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            connectMQTT();
        }
    } else {
        mqttClient.loop();
    }
    
    // Handle WiFi reconnection
    // 处理 WiFi 重连
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Disconnected, reconnecting...");
        currentLedMode = LED_CONNECTING;
        WiFi.reconnect();
        delay(5000);
    }
    
    delay(10);
}
