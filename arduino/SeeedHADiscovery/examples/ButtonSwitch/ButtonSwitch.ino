/**
 * ============================================================================
 * Seeed HA Discovery - 按钮开关示例
 * Button Switch Example
 * ============================================================================
 *
 * 这个示例展示如何：
 * 1. 检测物理按钮的三种按法（单击、双击、长按）
 * 2. 每种按法对应一个独立的开关状态
 * 3. 物理按钮和 Home Assistant 都可以控制开关状态
 * 4. 实时同步状态到 Home Assistant
 *
 * 硬件要求：
 * - XIAO ESP32-C3/C6/S3 或其他 ESP32 开发板
 * - 按钮（内置上拉电阻或外接上拉电阻）
 *
 * 按钮接线方法：
 * - 按钮一端 → GPIO (默认 D1)
 * - 按钮另一端 → GND
 * - 内部上拉电阻已启用
 *
 * 软件依赖：
 * - ArduinoJson (作者: Benoit Blanchon)
 * - WebSockets (作者: Markus Sattler)
 *
 * 使用方法：
 * 1. 修改下方的 WiFi 配置和按钮引脚
 * 2. 上传到 ESP32
 * 3. 打开串口监视器查看 IP 地址
 * 4. 在 Home Assistant 中添加设备
 * 5. 尝试按钮的不同按法，观察 HA 中开关状态变化
 *
 * 按键操作：
 * - 单击：切换"单击开关"状态
 * - 双击：切换"双击开关"状态
 * - 长按 (>1秒)：切换"长按开关"状态
 *
 * @author limengdu
 * @version 1.0.0
 */

#include <SeeedHADiscovery.h>

// =============================================================================
// 配置区域 - 请根据你的环境修改
// Configuration - Please modify according to your environment
// =============================================================================

// WiFi 配置
const char* WIFI_SSID = "你的WiFi名称";      // Your WiFi SSID
const char* WIFI_PASSWORD = "你的WiFi密码";  // Your WiFi password

// 按钮引脚
#define BUTTON_PIN D1

// 按钮检测参数
#define LONG_PRESS_TIME 1000      // 长按阈值（毫秒）
#define DOUBLE_CLICK_TIME 300     // 双击间隔（毫秒）

// =============================================================================
// 全局变量
// =============================================================================

SeeedHADiscovery ha;

// 三个开关，对应三种按法
SeeedHASwitch* singleClickSwitch;
SeeedHASwitch* doubleClickSwitch;
SeeedHASwitch* longPressSwitch;

// 按钮状态
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
unsigned long lastClickTime = 0;
uint8_t clickCount = 0;

// =============================================================================
// 辅助函数
// =============================================================================

/**
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

    // 检测按下
    if (lastButtonState == HIGH && currentState == LOW) {
        buttonPressTime = now;
    }

    // 检测释放
    if (lastButtonState == LOW && currentState == HIGH) {
        unsigned long pressDuration = now - buttonPressTime;

        if (pressDuration >= LONG_PRESS_TIME) {
            // 长按
            event = BUTTON_LONG;
            clickCount = 0;
        } else {
            // 短按，检测双击
            if (now - lastClickTime < DOUBLE_CLICK_TIME) {
                clickCount++;
            } else {
                clickCount = 1;
            }
            lastClickTime = now;
        }
    }

    // 检测双击超时
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
// Arduino 主程序
// =============================================================================

void setup() {
    // 初始化串口
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Seeed HA Discovery - 按钮开关示例");
    Serial.println("========================================");
    Serial.println();

    // 初始化按钮引脚
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.printf("按钮引脚: D1 (GPIO%d)\n", BUTTON_PIN);

    // 配置设备信息
    ha.setDeviceInfo(
        "按钮控制器",        // 设备名称
        "XIAO ESP32",        // 设备型号
        "1.0.0"              // 固件版本
    );

    ha.enableDebug(true);

    // 连接 WiFi
    Serial.println("正在连接 WiFi...");

    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("❌ WiFi 连接失败！");
        while (1) {
            delay(1000);
        }
    }

    Serial.println("✅ WiFi 连接成功！");
    Serial.printf("IP 地址: %s\n", ha.getLocalIP().toString().c_str());

    // =========================================================================
    // 创建三个开关，对应三种按法
    // =========================================================================

    singleClickSwitch = ha.addSwitch("single", "单击开关", "mdi:gesture-tap");
    doubleClickSwitch = ha.addSwitch("double", "双击开关", "mdi:gesture-double-tap");
    longPressSwitch = ha.addSwitch("long", "长按开关", "mdi:gesture-tap-hold");

    // 注册回调 - 当 HA 发送命令时执行
    singleClickSwitch->onStateChange([](bool state) {
        Serial.printf("HA 控制 [单击]: %s\n", state ? "开" : "关");
    });

    doubleClickSwitch->onStateChange([](bool state) {
        Serial.printf("HA 控制 [双击]: %s\n", state ? "开" : "关");
    });

    longPressSwitch->onStateChange([](bool state) {
        Serial.printf("HA 控制 [长按]: %s\n", state ? "开" : "关");
    });

    // =========================================================================
    // 完成初始化
    // =========================================================================

    Serial.println();
    Serial.println("========================================");
    Serial.println("  初始化完成！");
    Serial.println("========================================");
    Serial.println();
    Serial.println("在 Home Assistant 中添加设备:");
    Serial.println("  设置 → 设备与服务 → 添加集成");
    Serial.println("  搜索 'Seeed HA Discovery'");
    Serial.printf("  输入 IP: %s\n", ha.getLocalIP().toString().c_str());
    Serial.println();
    Serial.println("支持的按钮操作:");
    Serial.println("  - 单击：切换'单击开关'");
    Serial.println("  - 双击：切换'双击开关'");
    Serial.println("  - 长按 (>1秒)：切换'长按开关'");
    Serial.println();
    Serial.println("等待按钮事件...");
    Serial.println();
}

void loop() {
    // 必须调用！处理网络事件
    ha.handle();

    // 检测按钮事件
    ButtonEvent event = detectButtonEvent();

    // 如果有事件，切换对应开关的状态
    if (event != BUTTON_NONE) {
        const char* eventName = "未知";
        SeeedHASwitch* targetSwitch = nullptr;

        switch (event) {
            case BUTTON_SINGLE:
                eventName = "单击";
                targetSwitch = singleClickSwitch;
                break;
            case BUTTON_DOUBLE:
                eventName = "双击";
                targetSwitch = doubleClickSwitch;
                break;
            case BUTTON_LONG:
                eventName = "长按";
                targetSwitch = longPressSwitch;
                break;
            default:
                break;
        }

        if (targetSwitch) {
            // 切换状态
            bool newState = !targetSwitch->getState();

            Serial.printf("按钮事件: %s → 开关状态: %s\n", 
                         eventName, newState ? "开" : "关");

            // 更新开关状态（同步到 HA）
            targetSwitch->setState(newState);
        }
    }

    // 连接状态监控
    static unsigned long lastCheck = 0;
    static bool wasConnected = false;

    if (millis() - lastCheck > 5000) {
        lastCheck = millis();

        bool connected = ha.isHAConnected();
        if (connected != wasConnected) {
            Serial.println(connected ? "🟢 HA 已连接" : "🔴 HA 已断开");
            wasConnected = connected;
        }
    }
}

