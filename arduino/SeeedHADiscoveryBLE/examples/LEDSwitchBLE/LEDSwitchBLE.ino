/**
 * ============================================================================
 * Seeed HA Discovery BLE - LED 开关示例（双向通信）
 * LED Switch Example with Bidirectional BLE Communication
 * ============================================================================
 *
 * 这个示例展示如何：
 * 1. 通过 BLE 让 Home Assistant 控制 LED 开关
 * 2. 使用 GATT 服务实现双向通信
 * 3. 同时广播 BTHome 数据供传感器使用
 *
 * 工作原理：
 * - 设备作为 GATT 服务器，暴露控制特征值
 * - Home Assistant 连接设备并写入命令
 * - 设备接收命令后控制 LED 并通知状态变化
 *
 * 硬件要求：
 * - XIAO ESP32-C3/C6/S3 或 XIAO nRF52840
 * - 板载 LED 或外接 LED
 *
 * ⚠️ 注意：
 * - XIAO ESP32-C3 没有用户 LED，需要外接！
 * - 如果使用 ESP32-C3，请取消下方 EXTERNAL_LED 的注释
 *
 * 软件依赖：
 * - ESP32: NimBLE-Arduino (通过库管理器安装)
 * - nRF52840 mbed: ArduinoBLE (已内置)
 *
 * @author limengdu
 * @version 1.1.0
 */

#include <SeeedHADiscoveryBLE.h>

// =============================================================================
// 配置区域
// =============================================================================

// 设备名称（会显示在 Home Assistant 中）
const char* DEVICE_NAME = "XIAO LED 控制器";

// 广播间隔（毫秒）
const uint32_t ADVERTISE_INTERVAL = 5000;

// LED 配置
// 如果你的板子没有 LED_BUILTIN（如 XIAO ESP32-C3），取消下面的注释
// #define EXTERNAL_LED
// #define LED_PIN D0  // 外接 LED 引脚

#ifdef EXTERNAL_LED
    #define LED_BUILTIN LED_PIN
#endif

// XIAO 系列通常是低电平点亮
#define LED_ACTIVE_LOW true

// =============================================================================
// 全局变量
// =============================================================================

SeeedHADiscoveryBLE ble;
SeeedBLESwitch* ledSwitch;
SeeedBLESensor* ledStateSensor;  // LED 状态传感器（用于 BTHome 广播）

// =============================================================================
// 辅助函数
// =============================================================================

/**
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
// Arduino 主程序
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Seeed HA Discovery BLE - LED 开关示例");
    Serial.println("  (双向通信版本)");
    Serial.println("========================================");
    Serial.println();

    // 初始化 LED
    pinMode(LED_BUILTIN, OUTPUT);
    setLED(false);  // 初始关闭
    Serial.println("LED 引脚初始化完成");

    // 启用调试
    ble.enableDebug(true);

    // 初始化 BLE（启用控制功能）
    // 第二个参数 true 表示启用 GATT 双向通信
    if (!ble.begin(DEVICE_NAME, true)) {
        Serial.println("BLE 初始化失败！");
        while (1) delay(1000);
    }

    Serial.println("BLE 初始化成功！");

    // 添加 LED 状态传感器（用于 BTHome 广播，让 HA 能发现设备）
    ledStateSensor = ble.addSensor(BTHOME_BINARY_POWER);
    ledStateSensor->setState(false);  // 初始状态：关闭

    // 添加 LED 开关
    ledSwitch = ble.addSwitch("led", "板载 LED");

    // 注册回调：当 HA 发送控制命令时执行
    ledSwitch->onStateChange([](bool state) {
        setLED(state);
        ledStateSensor->setState(state);  // 同步更新传感器状态
        Serial.print("收到 HA 命令: LED -> ");
        Serial.println(state ? "开" : "关");
    });

    Serial.println("LED 开关和状态传感器已添加");

    // 开始广播
    ble.advertise();

    Serial.println();
    Serial.println("========================================");
    Serial.println("  初始化完成！");
    Serial.println("========================================");
    Serial.println();
    Serial.print("设备名称: ");
    Serial.println(DEVICE_NAME);
    Serial.print("MAC 地址: ");
    Serial.println(ble.getAddress());
    Serial.print("广播间隔: ");
    Serial.print(ADVERTISE_INTERVAL / 1000);
    Serial.println(" 秒");
    Serial.println();
    Serial.println("GATT 服务 UUID:");
    Serial.print("  控制服务: ");
    Serial.println(SEEED_CONTROL_SERVICE_UUID);
    Serial.print("  命令特征: ");
    Serial.println(SEEED_CONTROL_COMMAND_CHAR_UUID);
    Serial.print("  状态特征: ");
    Serial.println(SEEED_CONTROL_STATE_CHAR_UUID);
    Serial.println();
    Serial.println("等待 Home Assistant 连接...");
    Serial.println("提示: 使用 nRF Connect 或其他 BLE 工具可以测试");
    Serial.println();
    Serial.println("命令格式: [开关索引][状态]");
    Serial.println("  例如: 0x00 0x01 = 开关0 打开");
    Serial.println("        0x00 0x00 = 开关0 关闭");
    Serial.println();
}

void loop() {
    // 处理 BLE 事件（必须调用！）
    ble.loop();

    // 定期广播 BTHome 数据
    static unsigned long lastAdvertise = 0;
    if (millis() - lastAdvertise >= ADVERTISE_INTERVAL) {
        lastAdvertise = millis();
        ble.advertise();

        // 打印状态
        Serial.print("状态: LED=");
        Serial.print(ledSwitch->getState() ? "开" : "关");
        Serial.print(", 已连接=");
        Serial.println(ble.isConnected() ? "是" : "否");
    }
}

