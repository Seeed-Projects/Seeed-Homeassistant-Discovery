/**
 * ============================================================================
 * Seeed HA Discovery BLE - HA State Subscribe Example (Dynamic)
 * Seeed HA Discovery BLE - HA 状态订阅示例（动态版）
 * ============================================================================
 *
 * This example receives ANY entities you select in Home Assistant.
 * 本示例接收你在 Home Assistant 中选择的任何实体。
 *
 * Features | 特点:
 * ✓ Fully dynamic - no hardcoding needed | 完全动态 - 无需硬编码
 * ✓ Entity names shown automatically | 自动显示实体名称
 * ✓ Supports up to 16 entities | 最多支持 16 个实体
 * ✓ Real-time state updates | 实时状态更新
 *
 * How to use | 使用方法:
 * 1. Upload this code to your device | 上传代码到设备
 * 2. In HA, find the BLE device and click "Configure" 
 *    在 HA 中找到 BLE 设备并点击"配置"
 * 3. Select any entities you want to receive
 *    选择你想接收的任何实体
 * 4. Save - the device will show them automatically!
 *    保存 - 设备会自动显示它们！
 *
 * Hardware Requirements | 硬件要求:
 * - XIAO ESP32-C3/C6/S3 or XIAO nRF52840
 *
 * Software Dependencies | 软件依赖:
 * - ESP32: NimBLE-Arduino (install via Library Manager)
 * - nRF52840 mbed: ArduinoBLE (built-in)
 *
 * @author limengdu
 * @version 1.6.0
 */

#include <SeeedHADiscoveryBLE.h>

// =============================================================================
// Configuration | 配置区域
// =============================================================================

// Device name (displayed in Home Assistant)
// 设备名称（会显示在 Home Assistant 中）
const char* DEVICE_NAME = "XIAO HA State Monitor";

// Advertise interval (ms) | 广播间隔（毫秒）
const uint32_t ADVERTISE_INTERVAL = 5000;

// =============================================================================
// HA Entity Subscription - Fully Dynamic
// HA 实体订阅 - 完全动态
// =============================================================================
//
// This example receives ANY entities you select in Home Assistant.
// 本示例接收你在 Home Assistant 中选择的任何实体。
//
// No hardcoding needed! Just:
// 无需硬编码！只需：
// 1. Upload this code to your device
//    上传此代码到设备
// 2. In HA, go to device config and select entities
//    在 HA 中，进入设备配置并选择实体
// 3. The device will receive and display them automatically
//    设备会自动接收并显示它们
//
// Maximum entities: 16 (limited by BLE bandwidth)
// 最大实体数：16（受 BLE 带宽限制）

#define MAX_ENTITIES 16

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

SeeedHADiscoveryBLE ble;

// Add a generic binary sensor for BTHome discovery
// 添加一个通用二进制传感器用于 BTHome 发现
SeeedBLESensor* onlineSensor;

// Note: Use ble.getSubscribedEntityCount() to get the actual count
// 注意：使用 ble.getSubscribedEntityCount() 获取实际数量

// =============================================================================
// HA State Callback | HA 状态回调
// =============================================================================

/**
 * Called when Home Assistant pushes entity state
 * 当 Home Assistant 推送实体状态时调用
 *
 * @param entityIndex Index of the entity (0, 1, 2...) | 实体索引
 * @param entityId HA entity ID (e.g., "sensor.temperature") | HA 实体 ID
 * @param state State as string | 字符串形式的状态
 * @param numericValue Numeric value (if applicable) | 数值（如果适用）
 */
void onHAStateReceived(uint8_t entityIndex, const char* entityId, const char* state, float numericValue) {
    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║       HA State Update Received           ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.print("║ Index:  ");
    Serial.println(entityIndex);
    Serial.print("║ Entity: ");
    Serial.println(entityId);
    Serial.print("║ State:  ");
    Serial.println(state);
    Serial.print("║ Value:  ");
    Serial.println(numericValue, 2);
    Serial.println("╚══════════════════════════════════════════╝");

    // Note: Entity count is tracked automatically by the library
    // Use ble.getSubscribedEntityCount() to get current count
    // 注意：实体计数由库自动管理，使用 ble.getSubscribedEntityCount() 获取

    // Example: Do something based on entity type
    // 示例：根据实体类型执行操作
    
    // Check if it's a sensor (numeric value)
    // 检查是否是传感器（数值型）
    if (strstr(entityId, "sensor.") != NULL) {
        Serial.print("  -> Sensor value: ");
        Serial.println(numericValue, 2);
        
        // Example: Check if temperature is too high
        if (strstr(entityId, "temperature") != NULL && numericValue > 30.0) {
            Serial.println("  ⚠️ Temperature is high!");
        }
    }
    
    // Check if it's a switch/light (boolean)
    // 检查是否是开关/灯（布尔型）
    if (strstr(entityId, "switch.") != NULL || strstr(entityId, "light.") != NULL) {
        bool isOn = (strcmp(state, "on") == 0);
        Serial.print("  -> Switch/Light is: ");
        Serial.println(isOn ? "ON" : "OFF");
    }
    
    Serial.println();
}

// =============================================================================
// Arduino Main Program | Arduino 主程序
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  Seeed HA Discovery BLE - HA State Subscribe");
    Serial.println("==============================================");
    Serial.println();

    // Enable debug output | 启用调试输出
    ble.enableDebug(true);

    // Register HA state callback | 注册 HA 状态回调
    // This will be called whenever HA pushes entity states
    // 当 HA 推送实体状态时会调用此回调
    ble.onHAState(onHAStateReceived);
    
    Serial.println("HA state callback registered.");
    Serial.println("Waiting for entities from Home Assistant...");

    // Initialize BLE (enable control mode for bidirectional communication)
    // 初始化 BLE（启用控制模式以支持双向通信）
    if (!ble.begin(DEVICE_NAME, true)) {
        Serial.println("BLE initialization failed!");
        while (1) delay(1000);
    }

    Serial.println("BLE initialization successful!");

    // Add a generic binary sensor (just to help BTHome discovery)
    // This simply indicates the device is online/active
    // 添加一个通用二进制传感器（仅用于辅助 BTHome 发现）
    // 表示设备在线/活动状态
    onlineSensor = ble.addSensor(BTHOME_BINARY_GENERIC);
    onlineSensor->setState(true);  // Device is online | 设备在线

    // Start advertising | 开始广播
    ble.advertise();

    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║       Initialization Complete!           ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.print("║ Device: ");
    Serial.print(DEVICE_NAME);
    for (int i = strlen(DEVICE_NAME); i < 28; i++) Serial.print(' ');
    Serial.println("║");
    Serial.print("║ MAC: ");
    Serial.print(ble.getAddress());
    Serial.println("              ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.println("║ How to use:                              ║");
    Serial.println("║ 1. Find this device in Home Assistant    ║");
    Serial.println("║ 2. Click 'Configure' in device options   ║");
    Serial.println("║ 3. Select ANY entities you want          ║");
    Serial.println("║ 4. Save - they will appear here!         ║");
    Serial.println("║                                          ║");
    Serial.println("║ ✓ Entity names are shown automatically   ║");
    Serial.println("║ ✓ No hardcoding needed                   ║");
    Serial.println("║ ✓ Up to 16 entities supported            ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.println("║ Waiting for Home Assistant connection... ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();
}

void loop() {
    // Handle BLE events (must call!)
    // 处理 BLE 事件（必须调用！）
    ble.loop();

    // Periodically advertise BTHome data and print status
    // 定期广播 BTHome 数据并打印状态
    static unsigned long lastAdvertise = 0;
    if (millis() - lastAdvertise >= ADVERTISE_INTERVAL) {
        lastAdvertise = millis();
        ble.advertise();

        // Print current status | 打印当前状态
        Serial.println("┌──────────────────────────────────────────┐");
        Serial.println("│           Current Status                 │");
        Serial.println("├──────────────────────────────────────────┤");
        Serial.print("│ Connected: ");
        Serial.print(ble.isConnected() ? "Yes" : "No ");
        Serial.println("                            │");
        
        uint8_t entityCount = ble.getSubscribedEntityCount();
        Serial.print("│ Entities received: ");
        Serial.print(entityCount);
        Serial.println("                      │");
        Serial.println("├──────────────────────────────────────────┤");
        
        if (entityCount == 0) {
            Serial.println("│ (waiting for HA to push entities...)    │");
        } else {
            // Dynamically print ALL received entities
            // 动态打印所有收到的实体
            for (uint8_t i = 0; i < MAX_ENTITIES; i++) {
                SeeedBLEHAState* state = ble.getHAState(i);
                if (state && state->hasValue()) {
                    // Print entity ID (truncate if too long)
                    Serial.print("│ [");
                    Serial.print(i);
                    Serial.print("] ");
                    
                    const char* entityId = state->getEntityId();
                    int idLen = strlen(entityId);
                    
                    // Print entity ID - if too long, show "..." + last 22 chars
                    // 打印实体 ID - 如果太长，显示 "..." + 最后 22 个字符
                    const int maxDisplay = 25;
                    int printed = 0;
                    
                    if (idLen > maxDisplay) {
                        // Show "..." then the ending (most unique part)
                        Serial.print("...");
                        printed = 3;
                        int startPos = idLen - (maxDisplay - 3);
                        for (int j = startPos; entityId[j] != '\0' && printed < maxDisplay; j++, printed++) {
                            Serial.print(entityId[j]);
                        }
                    } else {
                        for (int j = 0; entityId[j] != '\0'; j++, printed++) {
                            Serial.print(entityId[j]);
                        }
                    }
                    // Pad with spaces
                    for (int j = printed; j < maxDisplay; j++) {
                        Serial.print(' ');
                    }
                    Serial.println("│");
                    
                    // Print state and value
                    Serial.print("│     = ");
                    Serial.print(state->getString());
                    Serial.print(" (");
                    Serial.print(state->getFloat(), 2);
                    Serial.print(")");
                    // Pad to align
                    int stateLen = strlen(state->getString()) + 8;
                    for (int j = stateLen; j < 30; j++) {
                        Serial.print(' ');
                    }
                    Serial.println("│");
                }
            }
        }
        
        Serial.println("└──────────────────────────────────────────┘");
        Serial.println();
    }
}

