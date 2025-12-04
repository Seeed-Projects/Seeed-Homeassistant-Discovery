/**
 * ============================================================================
 * Seeed HA Discovery BLE - 按钮示例
 * Button Example (BLE)
 * ============================================================================
 *
 * 这个示例展示如何：
 * 1. 通过 BLE 发送按钮事件到 Home Assistant
 * 2. 支持单击、双击、长按等事件
 * 3. 超低功耗设计
 *
 * 硬件要求：
 * - XIAO ESP32-C3/C6/S3 或 XIAO nRF52840
 * - 按钮接在指定引脚（默认 D1）
 *
 * 软件依赖：
 * - ESP32: NimBLE-Arduino (通过库管理器安装)
 * - nRF52840 mbed: ArduinoBLE (已内置)
 * - nRF52840 Adafruit: Bluefruit (已内置)
 *
 * @author limengdu
 * @version 1.0.0
 */

#include <SeeedHADiscoveryBLE.h>

// =============================================================================
// 配置区域
// =============================================================================

// 设备名称
const char* DEVICE_NAME = "XIAO 按钮";

// 按钮引脚
#define BUTTON_PIN D1

// 长按阈值（毫秒）
#define LONG_PRESS_TIME 1000

// 双击间隔（毫秒）
#define DOUBLE_CLICK_TIME 300

// =============================================================================
// 全局变量
// =============================================================================

SeeedHADiscoveryBLE ble;

// 三个开关，对应三种按法
SeeedBLESwitch* singleClickSwitch;
SeeedBLESwitch* doubleClickSwitch;
SeeedBLESwitch* longPressSwitch;

// 状态传感器（用于BTHome广播）
SeeedBLESensor* singleState;
SeeedBLESensor* doubleState;
SeeedBLESensor* longState;

// 按钮状态
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
unsigned long lastClickTime = 0;
uint8_t clickCount = 0;

// 广播间隔
unsigned long lastAdvertise = 0;
#define ADVERTISE_INTERVAL 5000  // 每5秒广播一次（用于发现）

// =============================================================================
// 按钮处理
// =============================================================================

BTHomeButtonEvent detectButtonEvent() {
    bool currentState = digitalRead(BUTTON_PIN);
    BTHomeButtonEvent event = BTHOME_BUTTON_NONE;
    unsigned long now = millis();

    // 检测按下
    if (lastButtonState == HIGH && currentState == LOW) {
        buttonPressTime = now;
    }

    // 检测释放
    if (lastButtonState == LOW && currentState == HIGH) {
        unsigned long pressDuration = now - buttonPressTime;

        if (pressDuration >= LONG_PRESS_TIME) {
            // 长按
            event = BTHOME_BUTTON_LONG_PRESS;
            clickCount = 0;
        } else {
            // 短按，检测双击/三击
            if (now - lastClickTime < DOUBLE_CLICK_TIME) {
                clickCount++;
            } else {
                clickCount = 1;
            }
            lastClickTime = now;
        }
    }

    // 检测双击/三击超时
    if (clickCount > 0 && now - lastClickTime > DOUBLE_CLICK_TIME) {
        switch (clickCount) {
            case 1:
                event = BTHOME_BUTTON_PRESS;
                break;
            case 2:
                event = BTHOME_BUTTON_DOUBLE;
                break;
            case 3:
            default:
                event = BTHOME_BUTTON_TRIPLE;
                break;
        }
        clickCount = 0;
    }

    lastButtonState = currentState;
    return event;
}

// =============================================================================
// Arduino 主程序
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Seeed HA Discovery BLE - 按钮示例");
    Serial.println("========================================");
    Serial.println();

    // 初始化按钮引脚
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.print("按钮引脚: D1 (GPIO");
    Serial.print(BUTTON_PIN);
    Serial.println(")");

    // 启用调试
    ble.enableDebug(true);

    // 初始化 BLE（启用控制功能）
    // 第二个参数 true 表示启用 GATT 双向通信
    if (!ble.begin(DEVICE_NAME, true)) {
        Serial.println("BLE 初始化失败！");
        while (1) delay(1000);
    }

    Serial.println("BLE 初始化成功！");

    // 添加状态传感器（用于BTHome广播，让HA显示状态）
    singleState = ble.addSensor(BTHOME_BINARY_POWER);
    singleState->setState(false);
    doubleState = ble.addSensor(BTHOME_BINARY_GENERIC);
    doubleState->setState(false);
    longState = ble.addSensor(BTHOME_BINARY_OPENING);  // 使用不同的类型区分
    longState->setState(false);

    // 添加三个开关，对应三种按法（GATT控制）
    singleClickSwitch = ble.addSwitch("single", "单击");
    doubleClickSwitch = ble.addSwitch("double", "双击");
    longPressSwitch = ble.addSwitch("long", "长按");

    // 注册回调：当 HA 远程控制时，同步更新传感器状态
    singleClickSwitch->onStateChange([](bool state) {
        singleState->setState(state);
        Serial.print("HA 控制 [单击]: ");
        Serial.println(state ? "开" : "关");
    });
    doubleClickSwitch->onStateChange([](bool state) {
        doubleState->setState(state);
        Serial.print("HA 控制 [双击]: ");
        Serial.println(state ? "开" : "关");
    });
    longPressSwitch->onStateChange([](bool state) {
        longState->setState(state);
        Serial.print("HA 控制 [长按]: ");
        Serial.println(state ? "开" : "关");
    });

    Serial.println("三个开关已添加");

    Serial.println();
    Serial.println("========================================");
    Serial.println("  初始化完成！");
    Serial.println("========================================");
    Serial.println();
    Serial.print("设备名称: ");
    Serial.println(DEVICE_NAME);
    Serial.print("MAC 地址: ");
    Serial.println(ble.getAddress());
    Serial.println();
    Serial.println("提示: 在 HA 中可通过 MAC 地址识别此设备");
    Serial.println();
    Serial.println("支持的按钮事件:");
    Serial.println("  - 单击");
    Serial.println("  - 双击");
    Serial.println("  - 三击");
    Serial.println("  - 长按 (>1秒)");
    Serial.println();
    Serial.println("等待按钮事件...");
    Serial.println("按钮触发后，对应的开关会切换状态");
    Serial.println();
}

void setLED(bool state) {
    // LED 反馈（如果有）
    // digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
}

void loop() {
    unsigned long now = millis();

    // 处理 BLE GATT 事件（必须调用）
    ble.loop();

    // 检测按钮事件
    BTHomeButtonEvent event = detectButtonEvent();

    // 如果有事件，切换对应开关的状态
    if (event != BTHOME_BUTTON_NONE) {
        const char* eventName = "未知";
        SeeedBLESwitch* targetSwitch = nullptr;

        switch (event) {
            case BTHOME_BUTTON_PRESS:
                eventName = "单击";
                targetSwitch = singleClickSwitch;
                break;
            case BTHOME_BUTTON_DOUBLE:
                eventName = "双击";
                targetSwitch = doubleClickSwitch;
                break;
            case BTHOME_BUTTON_TRIPLE:
                // 三击作为双击处理
                eventName = "三击(作为双击)";
                targetSwitch = doubleClickSwitch;
                break;
            case BTHOME_BUTTON_LONG_PRESS:
                eventName = "长按";
                targetSwitch = longPressSwitch;
                break;
            default:
                break;
        }

        if (targetSwitch) {
            // 切换开关状态
            bool newState = !targetSwitch->getState();
            targetSwitch->setState(newState);

            // 同步更新对应的传感器状态
            if (targetSwitch == singleClickSwitch) {
                singleState->setState(newState);
            } else if (targetSwitch == doubleClickSwitch) {
                doubleState->setState(newState);
            } else if (targetSwitch == longPressSwitch) {
                longState->setState(newState);
            }

            Serial.print("按钮事件: ");
            Serial.print(eventName);
            Serial.print(" -> 开关状态: ");
            Serial.println(newState ? "开" : "关");

            // 立即广播状态更新
            ble.advertise();
            lastAdvertise = now;
        }
    }

    // 定期广播，保持设备可被发现
    if (now - lastAdvertise >= ADVERTISE_INTERVAL) {
        ble.advertise();
        lastAdvertise = now;
    }

    delay(10);  // 按钮去抖
}
