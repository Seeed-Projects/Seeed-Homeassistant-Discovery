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
 * - 按钮接在指定引脚
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
SeeedBLESensor* button;
SeeedBLESensor* batterySensor;

// 按钮状态
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
unsigned long lastClickTime = 0;
uint8_t clickCount = 0;

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
    Serial.printf("按钮引脚: GPIO%d\n", BUTTON_PIN);

    // 启用调试
    ble.enableDebug(true);

    // 初始化 BLE
    if (!ble.begin(DEVICE_NAME)) {
        Serial.println("❌ BLE 初始化失败！");
        while (1) delay(1000);
    }

    Serial.println("✅ BLE 初始化成功！");

    // 添加按钮和电池传感器
    button = ble.addButton();
    batterySensor = ble.addBattery();
    batterySensor->setValue(100);  // 初始电量

    Serial.println("✅ 传感器已添加");

    Serial.println();
    Serial.println("========================================");
    Serial.println("  等待按钮事件...");
    Serial.println("========================================");
    Serial.println();
    Serial.println("支持的事件:");
    Serial.println("  - 单击");
    Serial.println("  - 双击");
    Serial.println("  - 三击");
    Serial.println("  - 长按 (>1秒)");
    Serial.println();
}

void loop() {
    // 检测按钮事件
    BTHomeButtonEvent event = detectButtonEvent();

    // 如果有事件，发送广播
    if (event != BTHOME_BUTTON_NONE) {
        button->triggerButton(event);
        ble.advertise();

        const char* eventName;
        switch (event) {
            case BTHOME_BUTTON_PRESS:
                eventName = "单击";
                break;
            case BTHOME_BUTTON_DOUBLE:
                eventName = "双击";
                break;
            case BTHOME_BUTTON_TRIPLE:
                eventName = "三击";
                break;
            case BTHOME_BUTTON_LONG_PRESS:
                eventName = "长按";
                break;
            default:
                eventName = "未知";
        }

        Serial.printf("📡 按钮事件: %s\n", eventName);
    }

    delay(10);  // 按钮去抖
}

