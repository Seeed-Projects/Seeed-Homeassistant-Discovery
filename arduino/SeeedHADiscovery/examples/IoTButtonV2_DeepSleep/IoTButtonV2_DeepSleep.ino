/**
 * ============================================================================
 * Seeed HA Discovery - IoT Button V2 Deep Sleep Example
 * Seeed HA Discovery - IoT Button V2 深度睡眠示例
 * ============================================================================
 *
 * This example demonstrates how to create a low-power IoT button with:
 * 本示例展示如何创建一个低功耗物联网按钮：
 * 1. Four button press detection modes (single, double, triple, long press)
 *    四种按键检测模式（单击、双击、三击、长按）
 * 2. Battery voltage monitoring with ADC
 *    电池电压监测，使用 ADC
 * 3. RGB LED effects (Blink, Rainbow, Subtle Flicker, Random Color)
 *    RGB LED 灯效（闪烁、彩虹渐变、微闪、随机颜色）
 * 4. Deep Sleep mode with GPIO wake-up for minimum power consumption (~10µA)
 *    深度睡眠模式，GPIO 唤醒，最低功耗（约 10µA）
 * 5. WiFi connectivity with Home Assistant integration
 *    WiFi 连接与 Home Assistant 集成
 * 6. Smart sleep timeout: 5s after HA connected, 3min before connection
 *    智能休眠超时：HA 连接后 5 秒，连接前 3 分钟
 * 7. Dev mode (triple click): 3 minute timeout for firmware upload
 *    开发模式（三击）：3 分钟超时，便于上传固件
 * 8. Seamless wake-up: wake-up press counts as first click in sequence
 *    无缝唤醒：唤醒按键作为序列中的第一次点击
 *
 * Hardware Platform:
 * 硬件平台：
 * - ESP32-C6 (esp32-c6-devkitc-1)
 * - Flash: 4MB, CPU: 80MHz (for low power)
 *   Flash: 4MB, CPU: 80MHz（降低功耗）
 *
 * Pin Configuration:
 * 引脚配置：
 * - GPIO0:  Battery voltage detection enable (HIGH = enabled)
 *           电池电压检测使能（HIGH = 使能）
 * - GPIO1:  Battery ADC (12dB attenuation, x3.0 for actual voltage)
 *           电池 ADC（12dB 衰减，乘 3.0 得实际电压）
 * - GPIO2:  Button (pull-up, inverted, wake source)
 *           按钮（上拉、反向逻辑、唤醒源）
 * - GPIO3:  Blue LED (inverted, LOW = ON)
 *           蓝色 LED（反向逻辑，LOW = 点亮）
 * - GPIO14: Red LED (inverted, LOW = ON)
 *           红色 LED（反向逻辑，LOW = 点亮）
 * - GPIO18: LED strip power enable (HIGH = enabled)
 *           LED 灯带电源使能（HIGH = 使能）
 * - GPIO19: WS2812 RGB LED data (single LED, GRB order)
 *           WS2812 RGB LED 数据线（单颗灯珠，GRB 颜色顺序）
 *
 * Button Functions:
 * 按钮功能：
 * - Single click: Toggle Switch 1 | 单击：切换开关 1
 * - Double click: Toggle Switch 2 | 双击：切换开关 2
 * - Triple click: Toggle Dev Mode (3 min sleep) | 三击：切换开发模式（3 分钟休眠）
 * - Long press (1-2s): Toggle Switch 3 | 长按（1-2秒）：切换开关 3
 *
 * Entities exposed to Home Assistant:
 * 暴露给 Home Assistant 的实体：
 * - Battery Voltage (sensor)
 *   电池电压（传感器）
 * - Battery Percentage (sensor)
 *   电池电量百分比（传感器）
 * - Switch 1/2/3 (switch)
 *   开关 1/2/3
 * - Button State (sensor)
 *   按钮状态（传感器）
 *
 * Software Dependencies:
 * 软件依赖：
 * - ArduinoJson (by Benoit Blanchon)
 * - WebSockets (by Markus Sattler)
 * - Adafruit NeoPixel (for WS2812 RGB LED)
 * - Preferences (ESP32 built-in, for persistent storage)
 *
 * @author limengdu
 * @version 1.0.0
 */

#include <SeeedHADiscovery.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_bt.h>         // For btStop() to disable Bluetooth | 用于 btStop() 禁用蓝牙

// =============================================================================
// Configuration - Please modify according to your environment
// 配置区域 - 请根据你的环境修改
// =============================================================================

// WiFi Configuration | WiFi 配置
const char* WIFI_SSID = "Your_WiFi_SSID";          // Your WiFi SSID | 你的WiFi名称
const char* WIFI_PASSWORD = "Your_WiFi_Password";  // Your WiFi password | 你的WiFi密码

// =============================================================================
// Pin Definitions | 引脚定义
// =============================================================================

// Output Pins | 输出引脚
#define PIN_BATTERY_EN      0   // Battery voltage detection enable (HIGH = enabled)
#define PIN_RED_LED         14  // Red LED (inverted, LOW = ON)
#define PIN_BLUE_LED        3   // Blue LED (inverted, LOW = ON)
#define PIN_LED_STRIP_EN    18  // LED strip power enable (HIGH = enabled)
#define PIN_RGB_LED         19  // WS2812 RGB LED data

// Input Pins | 输入引脚
#define PIN_BUTTON          2   // Button (pull-up, inverted, wake source)

// ADC Pins | ADC 引脚
#define PIN_BATTERY_ADC     1   // Battery ADC

// =============================================================================
// Timing Constants | 时间常量
// =============================================================================

// Button Detection Parameters | 按钮检测参数
#define LONG_PRESS_MIN_TIME     1000   // Long press minimum (ms) | 长按最小时间（毫秒）
#define LONG_PRESS_MAX_TIME     5000   // Long press maximum (ms) | 长按最大时间（毫秒）- 增加到5秒
#define SINGLE_CLICK_MAX_TIME   1000   // Single click max press time (ms) | 单击最大按下时间（毫秒）
#define SINGLE_CLICK_WAIT_TIME  500    // Wait time for single click confirmation (ms) | 单击确认等待时间（毫秒）
#define DOUBLE_CLICK_GAP_TIME   1000   // Double click max release gap (ms) | 双击最大释放间隔（毫秒）
#define DOUBLE_CLICK_MAX_PRESS  1000   // Double click max press time (ms) | 双击单次最大按下时间（毫秒）

// Battery Monitoring | 电池监测
#define BATTERY_VOLTAGE_MIN     2.75f  // 0% voltage | 0% 电压
#define BATTERY_VOLTAGE_MAX     4.2f   // 100% voltage | 100% 电压
#define BATTERY_ADC_MULTIPLIER  3.0f   // ADC to actual voltage multiplier | ADC 到实际电压乘数
#define BATTERY_JUMP_THRESHOLD  5.0f   // Anti-jump threshold (%) | 防跳变阈值（%）

// Sleep Parameters | 休眠参数
#define SLEEP_CHECK_INTERVAL           10000   // Sleep check interval (ms) | 休眠检查间隔（毫秒）
#define INACTIVITY_TIMEOUT_HA_CONNECTED 5000   // Sleep timeout after HA connected (ms) | HA连接后休眠超时（毫秒）
#define INACTIVITY_TIMEOUT_NOT_CONNECTED 180000 // Sleep timeout before HA connected (ms) | HA未连接时休眠超时（毫秒）
#define INACTIVITY_TIMEOUT_DEV_MODE    180000  // Dev mode sleep timeout (ms) | 开发模式休眠超时（毫秒）

// Triple Click Parameters | 三击参数
#define TRIPLE_CLICK_GAP_TIME   800    // Triple click max release gap (ms) | 三击最大释放间隔（毫秒）
#define TRIPLE_CLICK_MAX_PRESS  800    // Triple click max press time (ms) | 三击单次最大按下时间（毫秒）

// RGB LED Effect Duration | RGB LED 灯效持续时间
#define RGB_EFFECT_DURATION     1000   // Effect duration (ms) | 灯效持续时间（毫秒）

// =============================================================================
// RGB LED Effect Types | RGB LED 灯效类型
// =============================================================================
enum RGBEffect {
    RGB_EFFECT_NONE,
    RGB_EFFECT_BLINK,           // Pink-purple blink | 粉紫色闪烁
    RGB_EFFECT_RAINBOW,         // Rainbow gradient | 彩虹渐变
    RGB_EFFECT_SUBTLE_FLICKER,  // Subtle flicker | 微闪
    RGB_EFFECT_RANDOM_COLOR     // Random color gradient | 随机颜色渐变
};

// =============================================================================
// Button Event Types | 按钮事件类型
// =============================================================================
enum ButtonEvent {
    BUTTON_NONE,
    BUTTON_SINGLE,
    BUTTON_DOUBLE,
    BUTTON_TRIPLE,   // Triple click for dev mode | 三击进入开发模式
    BUTTON_LONG
};

// =============================================================================
// Global Objects | 全局对象
// =============================================================================

SeeedHADiscovery ha;
Adafruit_NeoPixel rgbLED(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
Preferences preferences;

// Sensors | 传感器
SeeedHASensor* batteryVoltageSensor;
SeeedHASensor* batteryPercentSensor;
SeeedHASensor* buttonStateSensor;

// Switches | 开关
SeeedHASwitch* switch1;
SeeedHASwitch* switch2;
SeeedHASwitch* switch3;

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

// Button state variables | 按钮状态变量
uint32_t button_press_time = 0;       // Button press timestamp | 按钮按下时刻
uint32_t last_activity_time = 0;      // Last activity timestamp (for sleep) | 最后活动时间（用于休眠）
bool lastButtonState = HIGH;          // Previous button state | 上一次按钮状态
uint8_t clickCount = 0;               // Click counter | 点击计数
uint32_t lastReleaseTime = 0;         // Last release timestamp | 上次释放时间

// Battery monitoring variables | 电池监测变量
float last_battery_percentage = 100.0f;  // Last battery percentage (persistent) | 上次电池百分比（持久化）

// HA sync state | HA 同步状态
bool haStatesSynced = false;  // Whether states have been synced to HA after boot | 启动后是否已同步状态到 HA

// Development mode | 开发模式
bool devModeEnabled = false;  // Dev mode for extended sleep timeout | 开发模式，延长休眠超时

// Wake-up button event handling | 唤醒按键事件处理
ButtonEvent pendingWakeupEvent = BUTTON_NONE;  // Event detected during boot | 启动时检测到的事件
bool wakeupEventProcessed = false;             // Whether wake-up event has been processed | 唤醒事件是否已处理

// RGB LED effect variables | RGB LED 灯效变量
RGBEffect currentEffect = RGB_EFFECT_NONE;
uint32_t effectStartTime = 0;
uint32_t effectColor = 0;  // For Subtle Flicker base color | 用于微闪的基础颜色

// WiFi state tracking | WiFi 状态跟踪
bool wasWiFiConnected = false;

// =============================================================================
// LED Control Functions | LED 控制函数
// =============================================================================

/**
 * Set Red LED state | 设置红色 LED 状态
 * Inverted logic: LOW = ON, HIGH = OFF
 * 反向逻辑：LOW = 点亮，HIGH = 熄灭
 */
void setRedLED(bool on) {
    digitalWrite(PIN_RED_LED, on ? LOW : HIGH);
}

/**
 * Set Blue LED state | 设置蓝色 LED 状态
 * Inverted logic: LOW = ON, HIGH = OFF
 * 反向逻辑：LOW = 点亮，HIGH = 熄灭
 */
void setBlueLED(bool on) {
    digitalWrite(PIN_BLUE_LED, on ? LOW : HIGH);
}

/**
 * Set Battery detection enable | 设置电池检测使能
 */
void setBatteryDetectEnable(bool enable) {
    digitalWrite(PIN_BATTERY_EN, enable ? HIGH : LOW);
}

/**
 * Set LED strip power enable | 设置 LED 灯带电源使能
 */
void setLEDStripPower(bool enable) {
    digitalWrite(PIN_LED_STRIP_EN, enable ? HIGH : LOW);
}

/**
 * Set RGB LED color | 设置 RGB LED 颜色
 * @param r Red (0-255) | 红色 (0-255)
 * @param g Green (0-255) | 绿色 (0-255)
 * @param b Blue (0-255) | 蓝色 (0-255)
 */
void setRGBLED(uint8_t r, uint8_t g, uint8_t b) {
    rgbLED.setPixelColor(0, rgbLED.Color(r, g, b));
    rgbLED.show();
}

/**
 * Turn off RGB LED | 关闭 RGB LED
 */
void turnOffRGBLED() {
    setRGBLED(0, 0, 0);
}

// =============================================================================
// RGB LED Effect Functions | RGB LED 灯效函数
// =============================================================================

/**
 * Start RGB LED effect | 启动 RGB LED 灯效
 */
void startRGBEffect(RGBEffect effect, uint32_t color = 0) {
    currentEffect = effect;
    effectStartTime = millis();
    effectColor = color;
}

/**
 * Update Blink effect | 更新闪烁灯效
 * Pink-purple: R=100%, G=0%, B=50%
 * 粉紫色：R=100%, G=0%, B=50%
 */
void updateBlinkEffect() {
    uint32_t elapsed = millis() - effectStartTime;
    // 500ms on, 500ms off cycle | 500ms 亮，500ms 灭循环
    bool ledOn = ((elapsed / 500) % 2) == 0;
    if (ledOn) {
        setRGBLED(255, 0, 128);  // Pink-purple | 粉紫色
    } else {
        turnOffRGBLED();
    }
}

/**
 * Update Rainbow effect | 更新彩虹渐变灯效
 */
void updateRainbowEffect() {
    uint32_t elapsed = millis() - effectStartTime;
    // Rainbow cycle over 1 second | 1秒内完成一次彩虹循环
    uint16_t hue = (elapsed * 65536 / RGB_EFFECT_DURATION) % 65536;
    uint32_t color = rgbLED.ColorHSV(hue, 255, 255);
    rgbLED.setPixelColor(0, color);
    rgbLED.show();
}

/**
 * Update Subtle Flicker effect | 更新微闪灯效
 */
void updateSubtleFlickerEffect() {
    uint32_t elapsed = millis() - effectStartTime;
    // Subtle brightness variation | 微弱亮度变化
    float phase = (float)elapsed / 100.0f;  // 100ms period | 100ms 周期
    float brightness = 0.7f + 0.3f * sin(phase * 3.14159f);
    
    uint8_t r = ((effectColor >> 16) & 0xFF) * brightness;
    uint8_t g = ((effectColor >> 8) & 0xFF) * brightness;
    uint8_t b = (effectColor & 0xFF) * brightness;
    setRGBLED(r, g, b);
}

/**
 * Update Random Color effect | 更新随机颜色渐变灯效
 */
void updateRandomColorEffect() {
    uint32_t elapsed = millis() - effectStartTime;
    // Random color transitions | 随机颜色过渡
    static uint16_t targetHue = random(65536);
    static uint16_t currentHue = 0;
    
    // Smoothly transition towards target | 平滑过渡到目标
    if (elapsed % 200 == 0) {  // Update target every 200ms | 每 200ms 更新目标
        targetHue = random(65536);
    }
    
    // Interpolate hue | 插值色相
    int16_t diff = targetHue - currentHue;
    currentHue += diff / 10;
    
    uint32_t color = rgbLED.ColorHSV(currentHue, 255, 200);
    rgbLED.setPixelColor(0, color);
    rgbLED.show();
}

/**
 * Update RGB LED effect | 更新 RGB LED 灯效
 * Call this in loop() | 在 loop() 中调用
 */
void updateRGBEffect() {
    if (currentEffect == RGB_EFFECT_NONE) {
        return;
    }
    
    // Check if effect duration exceeded | 检查灯效是否超时
    if (millis() - effectStartTime >= RGB_EFFECT_DURATION) {
        currentEffect = RGB_EFFECT_NONE;
        turnOffRGBLED();
        return;
    }
    
    // Update based on effect type | 根据灯效类型更新
    switch (currentEffect) {
        case RGB_EFFECT_BLINK:
            updateBlinkEffect();
            break;
        case RGB_EFFECT_RAINBOW:
            updateRainbowEffect();
            break;
        case RGB_EFFECT_SUBTLE_FLICKER:
            updateSubtleFlickerEffect();
            break;
        case RGB_EFFECT_RANDOM_COLOR:
            updateRandomColorEffect();
            break;
        default:
            break;
    }
}

// =============================================================================
// Battery Monitoring Functions | 电池监测函数
// =============================================================================

/**
 * Read battery voltage from ADC | 从 ADC 读取电池电压
 * IMPORTANT: GPIO0 must be HIGH to enable battery voltage detection circuit
 * 重要：GPIO0 必须为 HIGH 以使能电池电压检测电路
 * @return Actual battery voltage | 实际电池电压
 */
float readBatteryVoltage() {
    // Ensure battery detection is enabled | 确保电池检测已使能
    setBatteryDetectEnable(true);
    delay(10);  // Wait for circuit to settle | 等待电路稳定
    
    // Take multiple readings for accuracy | 多次采样提高精度
    uint32_t adcSum = 0;
    for (int i = 0; i < 10; i++) {
        adcSum += analogRead(PIN_BATTERY_ADC);
        delayMicroseconds(100);
    }
    
    float adcVoltage = ((float)adcSum / 10.0f / 4095.0f) * 3.3f;
    return adcVoltage * BATTERY_ADC_MULTIPLIER;
}

/**
 * Calculate battery percentage from voltage
 * 从电压计算电池百分比
 * Formula: percentage = ((voltage - 2.75) / (4.2 - 2.75)) * 100
 */
float calculateBatteryPercentage(float voltage) {
    if (isnan(voltage) || voltage < BATTERY_VOLTAGE_MIN || voltage > BATTERY_VOLTAGE_MAX) {
        return NAN;
    }
    float percentage = ((voltage - BATTERY_VOLTAGE_MIN) / (BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN)) * 100.0f;
    return constrain(percentage, 0.0f, 100.0f);
}

/**
 * Update battery readings | 更新电池读数
 * Called on boot and when button is pressed
 * 在启动和按钮按下时调用
 */
void updateBatteryReadings() {
    float voltage = readBatteryVoltage();
    
    // Round voltage to 2 decimal places | 电压四舍五入到2位小数
    voltage = round(voltage * 100.0f) / 100.0f;
    batteryVoltageSensor->setValue(voltage);
    
    float percentage = calculateBatteryPercentage(voltage);
    
    if (!isnan(percentage)) {
        // Round to integer | 四舍五入到整数
        percentage = round(percentage);
        
        // Anti-jump: if increase < 5%, keep old value
        // 防跳变：如果增量 < 5%，保持旧值
        if (percentage > last_battery_percentage && 
            (percentage - last_battery_percentage) < BATTERY_JUMP_THRESHOLD) {
            percentage = last_battery_percentage;
        } else {
            last_battery_percentage = percentage;
            preferences.putFloat("last_batt_pct", last_battery_percentage);
        }
        batteryPercentSensor->setValue(percentage);
    }
    
    Serial.printf("Battery: %.2fV, %.0f%%\n", voltage, percentage);
}

// =============================================================================
// Button Detection Functions | 按钮检测函数
// =============================================================================

/**
 * Update button state sensor | 更新按钮状态传感器
 */
void updateButtonState(bool pressed) {
    buttonStateSensor->setValue(pressed ? 1.0f : 0.0f);
}

/**
 * Detect button event | 检测按钮事件
 * 
 * Button detection logic as per requirements:
 * 按钮检测逻辑按照需求：
 * - Single click: press ≤ 1s, release, no press within 0.5s
 *   单击：按下不超过 1 秒后释放，且释放后 0.5 秒内无再次按下
 * - Double click: two press-release cycles (each press ≤ 1s, release gap ≤ 1s)
 *   双击：在短时间内完成两次按下释放（每次按下不超过 1 秒，中间释放不超过 1 秒）
 * - Triple click: three press-release cycles (each press ≤ 0.5s, release gap ≤ 0.5s) - Dev mode
 *   三击：在短时间内完成三次按下释放（每次按下不超过 0.5 秒，中间释放不超过 0.5 秒）- 开发模式
 * - Long press: press duration 1-2s
 *   长按：按下持续 1 到 2 秒后释放
 * 
 * @return Button event type | 按钮事件类型
 */
ButtonEvent detectButtonEvent() {
    bool currentState = digitalRead(PIN_BUTTON);
    ButtonEvent event = BUTTON_NONE;
    uint32_t now = millis();
    
    // Detect press (HIGH -> LOW transition)
    // 检测按下（HIGH -> LOW 跳变）
    if (lastButtonState == HIGH && currentState == LOW) {
        button_press_time = now;
        last_activity_time = now;  // Update activity time | 更新活动时间
        updateButtonState(true);
        Serial.println("Button pressed");
    }
    
    // Detect release (LOW -> HIGH transition)
    // 检测释放（LOW -> HIGH 跳变）
    if (lastButtonState == LOW && currentState == HIGH) {
        uint32_t pressDuration = now - button_press_time;
        last_activity_time = now;  // Update activity time | 更新活动时间
        updateButtonState(false);
        Serial.printf("Button released after %lu ms\n", pressDuration);
        
        // Determine press type | 判断按键类型
        if (pressDuration >= LONG_PRESS_MIN_TIME && pressDuration <= LONG_PRESS_MAX_TIME) {
            // Long press detected (1-5 seconds)
            // 检测到长按（1-5秒）
            event = BUTTON_LONG;
            clickCount = 0;
            lastReleaseTime = 0;
            Serial.println("Long press detected");
        } else if (pressDuration <= SINGLE_CLICK_MAX_TIME) {
            // Short press (≤ 1s), could be part of multi-click sequence
            // 短按（≤ 1秒），可能是多击序列的一部分
            if (clickCount >= 1 && (now - lastReleaseTime) <= DOUBLE_CLICK_GAP_TIME) {
                // Continue click sequence | 继续点击序列
                clickCount++;
                lastReleaseTime = now;
                
                if (clickCount >= 3) {
                    // Triple click detected! | 检测到三击！
                    event = BUTTON_TRIPLE;
                    clickCount = 0;
                    lastReleaseTime = 0;
                    Serial.println("Triple click detected - Dev mode!");
                }
                // If clickCount == 2, wait to see if it becomes triple click
                // 如果 clickCount == 2，等待看是否变成三击
            } else {
                // First click or gap too long | 第一次点击或间隔太长
                clickCount = 1;
                lastReleaseTime = now;
            }
        } else {
            // Press > 1s but < 1s minimum for long press - ignore
            // 按下时间 > 1s 但 < 长按最小时间 - 忽略
            clickCount = 0;
            lastReleaseTime = 0;
        }
    }
    
    // Check for single/double click confirmation (0.5s timeout with no next press)
    // 检查单击/双击确认（0.5秒内无下一次按下）
    if (clickCount >= 1 && currentState == HIGH && (now - lastReleaseTime >= SINGLE_CLICK_WAIT_TIME)) {
        if (clickCount == 1) {
            event = BUTTON_SINGLE;
            Serial.println("Single click detected");
        } else if (clickCount == 2) {
            event = BUTTON_DOUBLE;
            Serial.println("Double click detected");
        }
        clickCount = 0;
        lastReleaseTime = 0;
    }
    
    lastButtonState = currentState;
    return event;
}

/**
 * Detect button event during boot (for deep sleep wake-up)
 * 在启动过程中检测按键事件（用于深度睡眠唤醒）
 * 
 * This function is called during setup() when waking from deep sleep.
 * It counts the wake-up press as the first press in a click sequence.
 * 此函数在从深度睡眠唤醒时的 setup() 中调用。
 * 它将唤醒按键作为点击序列中的第一次按键。
 * 
 * Blocking function - waits until event is detected or timeout.
 * 阻塞函数 - 等待直到检测到事件或超时。
 * 
 * @return Detected button event | 检测到的按键事件
 */
ButtonEvent detectButtonEventDuringBoot() {
    // The button is already pressed (that's why we woke up)
    // Record the boot time as press start time
    // 按钮已经被按下（这就是我们唤醒的原因）
    // 将启动时间记录为按下开始时间
    uint32_t bootTime = millis();
    uint32_t pressStartTime = bootTime;
    uint8_t localClickCount = 0;
    uint32_t localLastReleaseTime = 0;
    
    // Maximum time to wait for complete button sequence | 等待完整按键序列的最大时间
    const uint32_t MAX_DETECTION_TIME = 6000;  // 6 seconds (to allow 5s long press) | 6秒（允许5秒长按）
    
    Serial.println("  Waiting for button release (first press)...");
    
    // Wait for first button release | 等待第一次按键释放
    while (digitalRead(PIN_BUTTON) == LOW) {
        delay(10);
        // Timeout check | 超时检查
        if (millis() - bootTime > MAX_DETECTION_TIME) {
            Serial.println("  Button held too long, timeout");
            return BUTTON_NONE;
        }
    }
    
    uint32_t firstRelease = millis();
    uint32_t firstPressDuration = firstRelease - pressStartTime;
    Serial.printf("  First press duration: %lu ms\n", firstPressDuration);
    
    // Check if it's a long press | 检查是否是长按
    if (firstPressDuration >= LONG_PRESS_MIN_TIME && firstPressDuration <= LONG_PRESS_MAX_TIME) {
        Serial.println("  Long press detected!");
        return BUTTON_LONG;
    }
    
    // Check if press is valid for click sequence | 检查按下是否有效
    if (firstPressDuration > SINGLE_CLICK_MAX_TIME) {
        // Too long but not a valid long press, ignore | 太长但不是有效的长按，忽略
        Serial.println("  Press too long, not valid");
        return BUTTON_NONE;
    }
    
    // Valid short press - count as first click | 有效的短按 - 计为第一次点击
    localClickCount = 1;
    localLastReleaseTime = firstRelease;
    
    // Now wait for potential additional clicks | 现在等待可能的额外点击
    Serial.println("  Waiting for additional clicks...");
    
    bool wasPressed = false;  // Track button state for edge detection | 跟踪按钮状态用于边沿检测
    
    while (millis() - bootTime < MAX_DETECTION_TIME) {
        uint32_t now = millis();
        bool buttonPressed = (digitalRead(PIN_BUTTON) == LOW);
        
        // Check for single click timeout | 检查单击超时
        if (localClickCount >= 1 && !buttonPressed && 
            (now - localLastReleaseTime >= SINGLE_CLICK_WAIT_TIME)) {
            
            if (localClickCount == 1) {
                Serial.println("  Single click confirmed!");
                return BUTTON_SINGLE;
            } else if (localClickCount == 2) {
                Serial.println("  Double click confirmed!");
                return BUTTON_DOUBLE;
            }
        }
        
        // Detect new press (button just pressed) | 检测新的按下（按钮刚被按下）
        if (buttonPressed && !wasPressed) {
            // New press detected | 检测到新的按下
            pressStartTime = now;
        }
        
        // Detect release (button just released) | 检测释放（按钮刚被释放）
        if (!buttonPressed && wasPressed) {
            uint32_t pressDuration = now - pressStartTime;
            
            Serial.printf("  Additional press duration: %lu ms, gap: %lu ms\n", 
                         pressDuration, now - localLastReleaseTime);
            
            // Check if this is part of a click sequence | 检查是否是点击序列的一部分
            // Use more generous timing for multi-click detection | 使用更宽松的时间检测多击
            if (pressDuration <= SINGLE_CLICK_MAX_TIME && 
                (now - localLastReleaseTime) <= DOUBLE_CLICK_GAP_TIME) {
                // Valid click for multi-click sequence | 有效的多击序列点击
                localClickCount++;
                localLastReleaseTime = now;
                Serial.printf("  Click count: %d\n", localClickCount);
                
                if (localClickCount >= 3) {
                    Serial.println("  Triple click detected!");
                    return BUTTON_TRIPLE;
                }
                // Continue waiting for more clicks | 继续等待更多点击
            } else if (pressDuration >= LONG_PRESS_MIN_TIME && 
                       pressDuration <= LONG_PRESS_MAX_TIME) {
                Serial.println("  Long press detected!");
                return BUTTON_LONG;
            } else {
                // Invalid click timing, reset | 无效的点击时序，重置
                Serial.println("  Invalid timing, resetting click count");
                localClickCount = 1;
                localLastReleaseTime = now;
            }
        }
        
        wasPressed = buttonPressed;  // Update state for next iteration | 更新状态用于下次迭代
        delay(10);
    }
    
    // Timeout - return whatever we have | 超时 - 返回我们有的
    if (localClickCount == 1) {
        return BUTTON_SINGLE;
    } else if (localClickCount == 2) {
        return BUTTON_DOUBLE;
    }
    
    return BUTTON_NONE;
}

// =============================================================================
// Switch Action Functions | 开关动作函数
// =============================================================================

/**
 * Handle Switch 1 toggle (Single click) | 处理开关1切换（单击）
 */
void handleSwitch1Toggle() {
    switch1->toggle();
    last_activity_time = millis();
    startRGBEffect(RGB_EFFECT_BLINK);
    updateBatteryReadings();  // Update battery on activity | 活动时更新电池
    Serial.printf("Switch 1: %s\n", switch1->getState() ? "ON" : "OFF");
}

/**
 * Handle Switch 2 toggle (Double click) | 处理开关2切换（双击）
 */
void handleSwitch2Toggle() {
    switch2->toggle();
    last_activity_time = millis();
    startRGBEffect(RGB_EFFECT_SUBTLE_FLICKER, 0xFF8000);  // Orange | 橙色
    updateBatteryReadings();
    Serial.printf("Switch 2: %s\n", switch2->getState() ? "ON" : "OFF");
}

/**
 * Handle Switch 3 toggle (Long press) | 处理开关3切换（长按）
 */
void handleSwitch3Toggle() {
    switch3->toggle();
    last_activity_time = millis();
    startRGBEffect(RGB_EFFECT_RAINBOW);
    updateBatteryReadings();
    Serial.printf("Switch 3: %s\n", switch3->getState() ? "ON" : "OFF");
}

/**
 * Handle Dev Mode toggle (Triple click) | 处理开发模式切换（三击）
 * Enables 3-minute sleep timeout for firmware upload
 * 启用 3 分钟休眠超时以便上传固件
 */
void handleDevModeToggle() {
    devModeEnabled = !devModeEnabled;
    last_activity_time = millis();
    
    if (devModeEnabled) {
        // Dev mode ON: Random color effect + both LEDs blink
        // 开发模式开启：随机颜色灯效 + 双灯闪烁
        startRGBEffect(RGB_EFFECT_RANDOM_COLOR);
        Serial.println("===========================================");
        Serial.println("  DEV MODE ENABLED - 3 minute sleep timeout");
        Serial.println("  开发模式已启用 - 3分钟休眠超时");
        Serial.println("===========================================");
        // Blink both LEDs to indicate dev mode | 双灯闪烁指示开发模式
        for (int i = 0; i < 3; i++) {
            setRedLED(true);
            setBlueLED(true);
            delay(100);
            setRedLED(false);
            setBlueLED(false);
            delay(100);
        }
        // Restore LED state based on WiFi | 根据 WiFi 状态恢复 LED
        if (ha.isWiFiConnected()) {
            setBlueLED(true);
        } else {
            setRedLED(true);
        }
    } else {
        // Dev mode OFF | 开发模式关闭
        Serial.println("DEV MODE DISABLED - Normal sleep timeout");
        Serial.println("开发模式已禁用 - 正常休眠超时");
    }
}

// =============================================================================
// WiFi Event Handling | WiFi 事件处理
// =============================================================================

/**
 * Handle WiFi connected event | 处理 WiFi 连接事件
 */
void onWiFiConnected() {
    Serial.println("WiFi connected!");
    setRedLED(false);   // Turn off red LED | 关闭红灯
    setBlueLED(true);   // Turn on blue LED | 打开蓝灯
    updateBatteryReadings();
}

/**
 * Handle WiFi disconnected event | 处理 WiFi 断开事件
 */
void onWiFiDisconnected() {
    Serial.println("WiFi disconnected!");
    setBlueLED(false);  // Turn off blue LED | 关闭蓝灯
    setRedLED(true);    // Turn on red LED | 打开红灯
}

/**
 * Check WiFi connection status | 检查 WiFi 连接状态
 */
void checkWiFiStatus() {
    bool isConnected = ha.isWiFiConnected();
    
    if (isConnected != wasWiFiConnected) {
        if (isConnected) {
            onWiFiConnected();
        } else {
            onWiFiDisconnected();
            haStatesSynced = false;  // Reset sync flag on disconnect | 断开时重置同步标志
        }
        wasWiFiConnected = isConnected;
    }
}

/**
 * Sync all states to Home Assistant | 同步所有状态到 Home Assistant
 * Called when HA connection is first established after boot
 * 在启动后首次建立 HA 连接时调用
 */
void syncAllStatesToHA() {
    Serial.println("Syncing all states to Home Assistant...");
    
    // Re-set all switch states to trigger notification to HA
    // 重新设置所有开关状态以触发向 HA 的通知
    switch1->setState(switch1->getState());
    switch2->setState(switch2->getState());
    switch3->setState(switch3->getState());
    
    // Update button state sensor | 更新按钮状态传感器
    buttonStateSensor->setValue(digitalRead(PIN_BUTTON) == LOW ? 1.0f : 0.0f);
    
    // Update battery readings | 更新电池读数
    updateBatteryReadings();
    
    Serial.printf("  Switch states: S1=%d, S2=%d, S3=%d\n", 
                  switch1->getState(), switch2->getState(), switch3->getState());
    Serial.println("State sync complete!");
}

/**
 * Check HA connection and sync states | 检查 HA 连接并同步状态
 */
void checkHAConnectionAndSync() {
    // If HA is connected and we haven't synced states yet, sync them
    // 如果 HA 已连接且我们尚未同步状态，则同步它们
    if (ha.isHAConnected() && !haStatesSynced) {
        syncAllStatesToHA();
        haStatesSynced = true;
    }
}

// =============================================================================
// Sleep Functions | 休眠函数
// =============================================================================

/**
 * Check if wake-up was from deep sleep | 检查是否从深度睡眠唤醒
 * @return true if woken from deep sleep | 如果从深度睡眠唤醒返回 true
 */
bool isWakeFromDeepSleep() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    return (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO || 
            wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ||
            wakeup_reason == ESP_SLEEP_WAKEUP_EXT1);
}

/**
 * Get wake-up reason string | 获取唤醒原因字符串
 */
const char* getWakeupReasonString() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:     return "EXT0 (RTC_IO)";
        case ESP_SLEEP_WAKEUP_EXT1:     return "EXT1 (RTC_CNTL)";
        case ESP_SLEEP_WAKEUP_TIMER:    return "Timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP:      return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO:     return "GPIO";
        case ESP_SLEEP_WAKEUP_UART:     return "UART";
        default:                        return "Power-on / Reset";
    }
}

/**
 * Prepare for deep sleep | 准备进入深度休眠
 * Minimize power consumption by disabling all peripherals
 * 通过禁用所有外设来最小化功耗
 */
void prepareForDeepSleep() {
    Serial.println("Preparing for deep sleep...");
    
    // Save switch states to persistent storage | 保存开关状态到持久化存储
    preferences.putBool("switch1", switch1->getState());
    preferences.putBool("switch2", switch2->getState());
    preferences.putBool("switch3", switch3->getState());
    Serial.println("  - Switch states saved to flash");
    
    // Turn off battery detection | 关闭电池检测
    setBatteryDetectEnable(false);
    Serial.println("  - Battery detection: DISABLED");
    
    // Turn off all LEDs | 关闭所有 LED
    setBlueLED(false);
    setRedLED(false);
    turnOffRGBLED();
    Serial.println("  - All LEDs: OFF");
    
    // Turn off LED strip power | 关闭 LED 灯带电源
    setLEDStripPower(false);
    Serial.println("  - LED strip power: DISABLED");
    
    // Disconnect WiFi to save power | 断开 WiFi 以节省电力
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("  - WiFi: DISABLED");
    
    // Disable Bluetooth (if enabled) | 禁用蓝牙（如果启用）
    btStop();
    Serial.println("  - Bluetooth: DISABLED");
    
    // Set all unused GPIO pins to input with pull-down to minimize leakage
    // 将所有未使用的 GPIO 引脚设置为带下拉的输入以最小化漏电
    // Note: Don't modify the wake-up pin (GPIO2)
    // 注意：不要修改唤醒引脚（GPIO2）
    
    // For output pins that are not the wake-up source, set them to known states
    // 对于不是唤醒源的输出引脚，将其设置为已知状态
    gpio_set_direction((gpio_num_t)PIN_BATTERY_EN, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_BATTERY_EN, GPIO_PULLDOWN_ONLY);
    
    gpio_set_direction((gpio_num_t)PIN_RED_LED, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_RED_LED, GPIO_PULLUP_ONLY);  // LED off state
    
    gpio_set_direction((gpio_num_t)PIN_BLUE_LED, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_BLUE_LED, GPIO_PULLUP_ONLY);  // LED off state
    
    gpio_set_direction((gpio_num_t)PIN_LED_STRIP_EN, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_LED_STRIP_EN, GPIO_PULLDOWN_ONLY);
    
    gpio_set_direction((gpio_num_t)PIN_RGB_LED, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_RGB_LED, GPIO_PULLDOWN_ONLY);
    
    Serial.println("  - GPIO pins configured for minimum leakage");
    Serial.println("  - All peripherals disabled for minimum power");
    Serial.println();
    Serial.println("Expected deep sleep current: ~10µA (ESP32-C6)");
}

/**
 * Configure deep sleep GPIO wake-up | 配置深度睡眠 GPIO 唤醒
 * For ESP32-C6, use esp_deep_sleep_enable_gpio_wakeup()
 * 对于 ESP32-C6，使用 esp_deep_sleep_enable_gpio_wakeup()
 */
void configureDeepSleepWakeUp() {
    // For ESP32-C6: Use GPIO wake-up for deep sleep
    // GPIO2 is the button pin, trigger on LOW level (button press)
    // 对于 ESP32-C6：使用 GPIO 唤醒深度睡眠
    // GPIO2 是按钮引脚，低电平触发（按钮按下）
    
    // Create a bitmask for GPIO2 | 创建 GPIO2 的位掩码
    uint64_t gpio_wakeup_mask = (1ULL << PIN_BUTTON);
    
    // Enable GPIO wake-up on LOW level | 在低电平时启用 GPIO 唤醒
    esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(gpio_wakeup_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    if (err == ESP_OK) {
        Serial.println("  - Wake-up source: GPIO2 (button, LOW level)");
    } else {
        Serial.printf("  - Warning: Failed to configure GPIO wake-up (error %d)\n", err);
        // Fallback: Try EXT0 wake-up if GPIO wake-up fails
        // 回退：如果 GPIO 唤醒失败，尝试 EXT0 唤醒
        #if SOC_PM_SUPPORT_EXT0_WAKEUP
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON, 0);  // 0 = LOW level
        Serial.println("  - Fallback: Using EXT0 wake-up on GPIO2");
        #endif
    }
    
    // Isolate GPIO to reduce power consumption | 隔离 GPIO 以降低功耗
    // Note: Don't isolate the wake-up pin | 注意：不要隔离唤醒引脚
    #if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED
    // Isolate other GPIOs if they support RTC | 如果支持 RTC，隔离其他 GPIO
    // rtc_gpio_isolate((gpio_num_t)PIN_RED_LED);    // Uncomment if needed
    // rtc_gpio_isolate((gpio_num_t)PIN_BLUE_LED);   // Uncomment if needed
    #endif
}

/**
 * Enter deep sleep | 进入深度睡眠
 * 
 * After deep sleep, the ESP32 will restart from setup()
 * 深度睡眠后，ESP32 将从 setup() 重新启动
 * 
 * Power consumption in deep sleep: ~10µA (ESP32-C6)
 * 深度睡眠功耗：约 10µA（ESP32-C6）
 */
void enterDeepSleep() {
    Serial.println();
    Serial.println("============================================");
    Serial.println("  Entering Deep Sleep Mode");
    Serial.println("============================================");
    
    // Notify HA before sleeping so it can start reconnecting immediately
    // 在休眠前通知 HA，让它可以立即开始重连
    ha.notifySleep();
    
    // Prepare for deep sleep | 准备深度睡眠
    prepareForDeepSleep();
    
    // Configure wake-up | 配置唤醒
    Serial.println("Configuring wake-up source...");
    configureDeepSleepWakeUp();
    
    // Flush serial before sleep | 睡眠前刷新串口
    Serial.println();
    Serial.println("Going to deep sleep now...");
    Serial.println("Press button to wake up!");
    Serial.println("============================================");
    Serial.flush();
    
    // Small delay to ensure everything is settled | 小延迟确保一切就绪
    delay(100);
    
    // Enter deep sleep - will NOT return from this function
    // ESP32 will restart from setup() when wake-up occurs
    // 进入深度睡眠 - 此函数不会返回
    // 当唤醒发生时，ESP32 将从 setup() 重新启动
    esp_deep_sleep_start();
    
    // Code below will never execute | 以下代码永远不会执行
}

/**
 * Get current sleep timeout based on connection state and dev mode
 * 根据连接状态和开发模式获取当前休眠超时
 */
uint32_t getCurrentSleepTimeout() {
    if (devModeEnabled) {
        return INACTIVITY_TIMEOUT_DEV_MODE;  // 3 minutes in dev mode | 开发模式 3 分钟
    } else if (ha.isHAConnected()) {
        return INACTIVITY_TIMEOUT_HA_CONNECTED;  // 5 seconds after HA connected | HA 连接后 5 秒
    } else {
        return INACTIVITY_TIMEOUT_NOT_CONNECTED;  // 3 minutes before HA connected | HA 未连接前 3 分钟
    }
}

/**
 * Check if device should sleep | 检查设备是否应该休眠
 */
void checkSleepCondition() {
    static uint32_t lastSleepCheck = 0;
    static uint32_t lastLoggedTimeout = 0;
    uint32_t now = millis();
    
    // Check every SLEEP_CHECK_INTERVAL | 每 SLEEP_CHECK_INTERVAL 检查一次
    if (now - lastSleepCheck < SLEEP_CHECK_INTERVAL) {
        return;
    }
    lastSleepCheck = now;
    
    // Get current timeout based on state | 根据状态获取当前超时
    uint32_t currentTimeout = getCurrentSleepTimeout();
    
    // Log timeout change | 记录超时变化
    if (currentTimeout != lastLoggedTimeout) {
        Serial.printf("Sleep timeout changed: %lu seconds ", currentTimeout / 1000);
        if (devModeEnabled) {
            Serial.println("(Dev mode)");
        } else if (ha.isHAConnected()) {
            Serial.println("(HA connected)");
        } else {
            Serial.println("(HA not connected)");
        }
        lastLoggedTimeout = currentTimeout;
    }
    
    // Check inactivity timeout | 检查不活动超时
    uint32_t inactiveTime = now - last_activity_time;
    
    if (inactiveTime >= currentTimeout) {
        Serial.printf("Inactive for %lu ms (timeout: %lu ms), entering deep sleep...\n", 
                      inactiveTime, currentTimeout);
        enterDeepSleep();
        // Note: Code after this point will never execute
        // 注意：此点之后的代码永远不会执行
    }
}

// =============================================================================
// Persistent Storage Functions | 持久化存储函数
// =============================================================================

/**
 * Initialize persistent storage | 初始化持久化存储
 */
void initPersistentStorage() {
    preferences.begin("iot_button", false);
    
    // Load last battery percentage | 加载上次电池百分比
    last_battery_percentage = round(preferences.getFloat("last_batt_pct", 100.0f));
    Serial.printf("Loaded last_battery_percentage: %.0f%%\n", last_battery_percentage);
    
    // Load switch states | 加载开关状态
    // This will be done after switches are created in setup()
}

// =============================================================================
// Arduino Main Program | Arduino 主程序
// =============================================================================

void setup() {
    // =========================================================================
    // CRITICAL: Initialize button pin FIRST for accurate detection
    // 关键：首先初始化按钮引脚以确保准确检测
    // =========================================================================
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    
    // Check wake-up reason BEFORE any delays | 在任何延迟之前检查唤醒原因
    bool wokeFromDeepSleep = isWakeFromDeepSleep();
    
    // If woke from deep sleep, detect button event IMMEDIATELY
    // 如果从深度睡眠唤醒，立即检测按键事件
    // This must happen before Serial.begin() delay to catch fast double/triple clicks
    // 必须在 Serial.begin() 延迟之前，以捕获快速的双击/三击
    if (wokeFromDeepSleep) {
        // Minimal serial init for debug (no delay) | 最小串口初始化用于调试（无延迟）
        Serial.begin(115200);
        Serial.println("\n[Wake-up] Detecting button event...");
        
        pendingWakeupEvent = detectButtonEventDuringBoot();
        
        if (pendingWakeupEvent != BUTTON_NONE) {
            Serial.printf("[Wake-up] Detected: %s\n", 
                pendingWakeupEvent == BUTTON_SINGLE ? "SINGLE" :
                pendingWakeupEvent == BUTTON_DOUBLE ? "DOUBLE" :
                pendingWakeupEvent == BUTTON_TRIPLE ? "TRIPLE" :
                pendingWakeupEvent == BUTTON_LONG ? "LONG" : "UNKNOWN");
        } else {
            Serial.println("[Wake-up] No valid event detected");
        }
    } else {
        // Fresh boot - can have delay for serial | 全新启动 - 可以等待串口
        Serial.begin(115200);
        delay(500);
    }
    
    Serial.println();
    Serial.println("============================================");
    Serial.println("  Seeed HA Discovery - IoT Button V2");
    Serial.println("  Deep Sleep Mode Example");
    Serial.println("============================================");
    
    Serial.printf("Boot reason: %s\n", getWakeupReasonString());
    
    if (wokeFromDeepSleep) {
        Serial.println("*** Woke up from deep sleep! ***");
    } else {
        Serial.println("*** Fresh boot / Power-on ***");
    }
    Serial.println();
    
    // Initialize persistent storage | 初始化持久化存储
    initPersistentStorage();
    
    // =========================================================================
    // Initialize other pins | 初始化其他引脚
    // =========================================================================
    
    // Output pins | 输出引脚
    pinMode(PIN_BATTERY_EN, OUTPUT);
    pinMode(PIN_RED_LED, OUTPUT);
    pinMode(PIN_BLUE_LED, OUTPUT);
    pinMode(PIN_LED_STRIP_EN, OUTPUT);
    
    // ADC configuration | ADC 配置
    analogSetAttenuation(ADC_11db);  // 12dB attenuation for full range | 12dB 衰减以获得完整范围
    
    // =========================================================================
    // Boot initialization sequence | 启动初始化序列
    // =========================================================================
    
    Serial.println("Boot initialization sequence:");
    
    // 1. Turn off blue LED | 关闭蓝色 LED
    setBlueLED(false);
    Serial.println("  - Blue LED: OFF");
    
    // 2. Turn on red LED | 打开红色 LED
    setRedLED(true);
    Serial.println("  - Red LED: ON");
    
    // 3. Enable battery detection | 使能电池检测
    setBatteryDetectEnable(true);
    Serial.println("  - Battery detection: ENABLED");
    
    // 4. Enable LED strip power | 使能 LED 灯带电源
    setLEDStripPower(true);
    Serial.println("  - LED strip power: ENABLED");
    
    // =========================================================================
    // Initialize RGB LED | 初始化 RGB LED
    // =========================================================================
    
    rgbLED.begin();
    rgbLED.setBrightness(100);
    turnOffRGBLED();
    Serial.println("  - RGB LED: Initialized");
    
    // =========================================================================
    // Configure device info | 配置设备信息
    // =========================================================================
    
    ha.setDeviceInfo(
        "IoT Button V2",         // Device name | 设备名称
        "ESP32-C6",              // Device model | 设备型号
        "1.0.0"                  // Firmware version | 固件版本
    );
    
    ha.enableDebug(true);
    
    // =========================================================================
    // Connect to WiFi | 连接 WiFi
    // =========================================================================
    
    Serial.println();
    Serial.println("Connecting to WiFi...");
    Serial.print("  SSID: ");
    Serial.println(WIFI_SSID);
    
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi connection failed!");
        // Blink red LED to indicate error | 红灯闪烁指示错误
        while (1) {
            setRedLED(true);
            delay(200);
            setRedLED(false);
            delay(200);
        }
    }
    
    Serial.println("WiFi connected!");
    Serial.print("  IP Address: ");
    Serial.println(ha.getLocalIP().toString().c_str());
    
    // Update LED state for WiFi connected | 更新 WiFi 连接后的 LED 状态
    setRedLED(false);
    setBlueLED(true);
    wasWiFiConnected = true;
    
    // =========================================================================
    // Create sensors | 创建传感器
    // =========================================================================
    
    Serial.println();
    Serial.println("Creating sensors...");
    
    // Battery voltage sensor | 电池电压传感器
    batteryVoltageSensor = ha.addSensor("battery_voltage", "Battery Voltage", "voltage", "V");
    batteryVoltageSensor->setPrecision(2);
    batteryVoltageSensor->setIcon("mdi:battery-charging");
    Serial.println("  - Battery Voltage sensor created");
    
    // Battery percentage sensor | 电池百分比传感器
    batteryPercentSensor = ha.addSensor("battery_percent", "Battery Percentage", "battery", "%");
    batteryPercentSensor->setPrecision(0);
    batteryPercentSensor->setIcon("mdi:battery");
    Serial.println("  - Battery Percentage sensor created");
    
    // Button state sensor | 按钮状态传感器
    buttonStateSensor = ha.addSensor("button_state", "Button State", "", "");
    buttonStateSensor->setPrecision(0);
    buttonStateSensor->setIcon("mdi:gesture-tap-button");
    buttonStateSensor->setValue(0);  // Initial state: not pressed | 初始状态：未按下
    Serial.println("  - Button State sensor created");
    
    // =========================================================================
    // Create switches | 创建开关
    // =========================================================================
    
    Serial.println();
    Serial.println("Creating switches...");
    
    // Switch 1 (Single click) | 开关1（单击）
    switch1 = ha.addSwitch("switch1", "Switch 1", "mdi:gesture-tap");
    switch1->onStateChange([](bool state) {
        Serial.printf("HA Control [Switch 1]: %s\n", state ? "ON" : "OFF");
        last_activity_time = millis();
        // Save state | 保存状态
        preferences.putBool("switch1", state);
    });
    Serial.println("  - Switch 1 created");
    
    // Switch 2 (Double click) | 开关2（双击）
    switch2 = ha.addSwitch("switch2", "Switch 2", "mdi:gesture-double-tap");
    switch2->onStateChange([](bool state) {
        Serial.printf("HA Control [Switch 2]: %s\n", state ? "ON" : "OFF");
        last_activity_time = millis();
        // Save state | 保存状态
        preferences.putBool("switch2", state);
    });
    Serial.println("  - Switch 2 created");
    
    // Switch 3 (Long press) | 开关3（长按）
    switch3 = ha.addSwitch("switch3", "Switch 3", "mdi:gesture-tap-hold");
    switch3->onStateChange([](bool state) {
        Serial.printf("HA Control [Switch 3]: %s\n", state ? "ON" : "OFF");
        last_activity_time = millis();
        // Save state | 保存状态
        preferences.putBool("switch3", state);
    });
    Serial.println("  - Switch 3 created");
    
    // Load saved switch states | 加载保存的开关状态
    bool s1State = preferences.getBool("switch1", false);
    bool s2State = preferences.getBool("switch2", false);
    bool s3State = preferences.getBool("switch3", false);
    
    switch1->setState(s1State);
    switch2->setState(s2State);
    switch3->setState(s3State);
    Serial.printf("  - Restored states: S1=%d, S2=%d, S3=%d\n", s1State, s2State, s3State);
    
    // =========================================================================
    // Initial battery reading | 初始电池读数
    // =========================================================================
    
    Serial.println();
    Serial.println("Reading initial battery status...");
    delay(100);  // Wait for ADC to stabilize | 等待 ADC 稳定
    updateBatteryReadings();
    
    // Initialize activity timer | 初始化活动计时器
    last_activity_time = millis();
    
    // =========================================================================
    // Initialization complete | 初始化完成
    // =========================================================================
    
    Serial.println();
    Serial.println("============================================");
    Serial.println("  Initialization Complete!");
    Serial.println("============================================");
    Serial.println();
    Serial.println("Add device in Home Assistant:");
    Serial.println("  Settings -> Devices & Services -> Add Integration");
    Serial.println("  Search 'Seeed HA Discovery'");
    Serial.print("  Enter IP: ");
    Serial.println(ha.getLocalIP().toString().c_str());
    Serial.println();
    Serial.println("Button operations:");
    Serial.println("  - Single click: Toggle Switch 1 + Blink effect");
    Serial.println("  - Double click: Toggle Switch 2 + Subtle Flicker (orange)");
    Serial.println("  - Triple click: Toggle Dev Mode (3 min sleep timeout)");
    Serial.println("  - Long press (1-2s): Toggle Switch 3 + Rainbow effect");
    Serial.println();
    Serial.println("Sleep timeouts:");
    Serial.printf("  - HA connected: %d seconds\n", INACTIVITY_TIMEOUT_HA_CONNECTED / 1000);
    Serial.printf("  - HA not connected: %d seconds\n", INACTIVITY_TIMEOUT_NOT_CONNECTED / 1000);
    Serial.printf("  - Dev mode: %d seconds\n", INACTIVITY_TIMEOUT_DEV_MODE / 1000);
    Serial.println();
    Serial.println("Waiting for events...");
    Serial.println();
}

void loop() {
    // Must call! Handle network events | 必须调用！处理网络事件
    ha.handle();
    
    // Check WiFi status | 检查 WiFi 状态
    checkWiFiStatus();
    
    // Check HA connection and sync states if needed | 检查 HA 连接并在需要时同步状态
    checkHAConnectionAndSync();
    
    // Process pending wake-up event after HA is connected | 在 HA 连接后处理待处理的唤醒事件
    if (pendingWakeupEvent != BUTTON_NONE && !wakeupEventProcessed && ha.isHAConnected()) {
        Serial.println("Processing pending wake-up event...");
        ButtonEvent eventToProcess = pendingWakeupEvent;
        pendingWakeupEvent = BUTTON_NONE;
        wakeupEventProcessed = true;
        
        switch (eventToProcess) {
            case BUTTON_SINGLE: handleSwitch1Toggle(); break;
            case BUTTON_DOUBLE: handleSwitch2Toggle(); break;
            case BUTTON_TRIPLE: handleDevModeToggle(); break;
            case BUTTON_LONG:   handleSwitch3Toggle(); break;
            default: break;
        }
    }
    
    // Detect and handle button events | 检测和处理按钮事件
    ButtonEvent event = detectButtonEvent();
    if (event != BUTTON_NONE) {
        switch (event) {
            case BUTTON_SINGLE: handleSwitch1Toggle(); break;
            case BUTTON_DOUBLE: handleSwitch2Toggle(); break;
            case BUTTON_TRIPLE: handleDevModeToggle(); break;
            case BUTTON_LONG:   handleSwitch3Toggle(); break;
            default: break;
        }
    }
    
    // Update RGB LED effect | 更新 RGB LED 灯效
    updateRGBEffect();
    
    // Check sleep condition | 检查休眠条件
    checkSleepCondition();
    
    delay(10);
}

