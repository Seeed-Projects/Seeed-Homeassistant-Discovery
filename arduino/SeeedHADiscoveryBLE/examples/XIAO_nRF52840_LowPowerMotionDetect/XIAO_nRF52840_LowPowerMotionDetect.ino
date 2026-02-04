/**
 * ============================================================================
 * Seeed HA Discovery BLE - Low Power Motion Detection Example
 * Seeed HA Discovery BLE - 低功耗运动检测示例
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Detect motion via LSM6DS3 IMU Z-axis acceleration
 *    通过 LSM6DS3 IMU 的 Z 轴加速度检测运动
 * 2. Broadcast motion status to Home Assistant via BTHome protocol
 *    通过 BTHome 协议向 Home Assistant 广播运动状态
 * 3. Enter ultra-low power deep sleep (System OFF) when idle
 *    空闲时进入超低功耗深度睡眠（System OFF）
 * 4. Wake up automatically when motion is detected
 *    检测到运动时自动唤醒
 *
 * Hardware Requirements:
 * 硬件要求：
 * - Seeed XIAO nRF52840 (with built-in LSM6DS3 IMU)
 *   Seeed XIAO nRF52840（内置 LSM6DS3 IMU）
 *
 * Software Dependencies:
 * 软件依赖：
 * - Seeed nRF52 Boards (Adafruit BSP)
 *   Seeed nRF52 开发板包（Adafruit BSP）
 * - Seeed Arduino LSM6DS3 library
 *   Seeed Arduino LSM6DS3 库
 *
 * Board Selection in Arduino IDE:
 * Arduino IDE 中的开发板选择：
 * Tools -> Board -> Seeed nRF52 Boards -> Seeed XIAO nRF52840
 * 工具 -> 开发板 -> Seeed nRF52 Boards -> Seeed XIAO nRF52840
 *
 * Power Consumption:
 * 功耗：
 * - Deep Sleep: < 5 µA
 *   深度睡眠：< 5 µA
 * - Advertising: ~3 mA
 *   广播中：~3 mA
 *
 * IMPORTANT: Disconnect USB to measure actual power consumption!
 * 重要：测量实际功耗时必须断开 USB 连接！
 *
 * @author limengdu
 * @version 1.0.0
 */

#include <LSM6DS3.h>
#include <Wire.h>
#include <bluefruit.h>

// =============================================================================
// Hardware Pin Definitions | 硬件引脚定义
// =============================================================================

// IMU interrupt pin (INT1)
// IMU 中断引脚 (INT1)
#define IMU_INT1_PIN 11  // P0.11

// LED pin definitions (active LOW for Adafruit BSP)
// LED 引脚定义（Adafruit BSP 使用低电平点亮）
#define LED_BLUE_PIN  12
#define LED_GREEN_PIN 13

// =============================================================================
// BTHome Protocol Definitions | BTHome 协议定义
// =============================================================================

// BTHome v2 device info byte: trigger-based device, no encryption
// BTHome v2 设备信息字节：触发型设备，无加密
#define BTHOME_DEVICE_INFO    0x44

// BTHome binary motion sensor object ID
// BTHome 二进制运动传感器对象 ID
#define BTHOME_BINARY_MOTION  0x21

// BLE GAP AD type for service data (16-bit UUID)
// BLE GAP AD 类型：服务数据（16 位 UUID）
#define BLE_GAP_AD_TYPE_SERVICE_DATA 0x16

// =============================================================================
// Configuration - Adjustable Parameters | 配置 - 可调参数
// =============================================================================

// Z-axis acceleration threshold for motion detection (in g)
// 运动检测的 Z 轴加速度阈值（单位：g）
const float ACCEL_THRESHOLD_Z = 1.30;

// Debounce time: continuous still duration before switching to clear state (ms)
// 防抖时间：切换到静止状态前需要持续静止的时间（毫秒）
const unsigned long DEBOUNCE_TIME = 6000;

// Clear duration: how long to broadcast motion=0 before sleep (ms)
// 清除持续时间：进入睡眠前广播 motion=0 的时间（毫秒）
const unsigned long CLEAR_DURATION = 5000;

// Advertising refresh interval during active state (ms)
// 活动状态下广播刷新间隔（毫秒）
const unsigned long ADVERTISE_INTERVAL = 100;

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

// IMU object (I2C address 0x6A)
// IMU 对象（I2C 地址 0x6A）
LSM6DS3 myIMU(I2C_MODE, 0x6A);

// Interrupt counter for wake-up detection
// 唤醒检测的中断计数器
volatile uint8_t interruptCount = 0;

// Device name and MAC address strings
// 设备名称和 MAC 地址字符串
char deviceName[20] = {0};
char macStr[18] = {0};  // "AA:BB:CC:DD:EE:FF"

// BLE state flags
// BLE 状态标志
bool bleStarted = false;
bool isMotionState = true;  // Current broadcast state: motion=1 or motion=0
                            // 当前广播状态：motion=1 或 motion=0

// Timing variables
// 计时变量
unsigned long lastMotionTime = 0;
unsigned long lastStillTime = 0;
unsigned long lastAdvertiseTime = 0;

// =============================================================================
// Interrupt Service Routine | 中断服务程序
// =============================================================================

/**
 * IMU INT1 interrupt handler
 * IMU INT1 中断处理程序
 */
void int1ISR() {
    interruptCount++;
}

// =============================================================================
// LED Control Functions | LED 控制函数
// =============================================================================

/**
 * Set green LED state (active LOW)
 * 设置绿色 LED 状态（低电平点亮）
 */
void setGreenLed(bool on) {
    digitalWrite(LED_GREEN_PIN, on ? LOW : HIGH);
}

/**
 * Set blue LED state (active LOW)
 * 设置蓝色 LED 状态（低电平点亮）
 */
void setBlueLed(bool on) {
    digitalWrite(LED_BLUE_PIN, on ? LOW : HIGH);
}

/**
 * Turn off all LEDs
 * 关闭所有 LED
 */
void ledsOff() {
    setGreenLed(false);
    setBlueLed(false);
}

// =============================================================================
// BTHome BLE Functions | BTHome BLE 函数
// =============================================================================

/**
 * Update BLE advertising data with motion state
 * 使用运动状态更新 BLE 广播数据
 * 
 * @param motionDetected true for motion=1, false for motion=0
 *                       true 表示 motion=1，false 表示 motion=0
 */
void updateAdvertising(bool motionDetected) {
    Bluefruit.Advertising.stop();
    Bluefruit.Advertising.clearData();
    
    // Build BTHome Service Data
    // 构建 BTHome 服务数据
    uint8_t serviceData[8];
    uint8_t len = 0;
    serviceData[len++] = 0xD2;  // UUID 0xFCD2 (little-endian)
    serviceData[len++] = 0xFC;
    serviceData[len++] = BTHOME_DEVICE_INFO;
    serviceData[len++] = BTHOME_BINARY_MOTION;
    serviceData[len++] = motionDetected ? 0x01 : 0x00;
    
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, serviceData, len);
    Bluefruit.Advertising.setInterval(32, 32);  // Fast advertising (20ms interval)
                                                 // 快速广播（20ms 间隔）
    Bluefruit.Advertising.start(0);
}

/**
 * Stop BLE advertising
 * 停止 BLE 广播
 */
void stopBLE() {
    if (bleStarted) {
        Bluefruit.Advertising.stop();
        delay(50);
        Serial.println("BLE stopped");
    }
}

// =============================================================================
// IMU Wake-up Configuration | IMU 唤醒配置
// =============================================================================

/**
 * Configure IMU wake-up interrupt for ultra-low power mode
 * 为超低功耗模式配置 IMU 唤醒中断
 */
void setupWakeUpInterrupt() {
    // 1. Configure accelerometer: 12.5Hz, 2g (lowest power)
    // 1. 配置加速度计：12.5Hz, 2g（最低功耗）
    myIMU.writeRegister(LSM6DS3_ACC_GYRO_CTRL1_XL, 0x10);
    
    // 2. Disable gyroscope
    // 2. 关闭陀螺仪
    myIMU.writeRegister(LSM6DS3_ACC_GYRO_CTRL2_G, 0x00);
    
    // 3. Enable interrupt, detect Z-axis only
    // 3. 启用中断，仅检测 Z 轴
    myIMU.writeRegister(LSM6DS3_ACC_GYRO_TAP_CFG1, 0x83);
    
    // 4. Wake-up threshold
    // 4. 唤醒阈值
    myIMU.writeRegister(LSM6DS3_ACC_GYRO_WAKE_UP_THS, 0x0A);
    
    // 5. Wake-up duration
    // 5. 唤醒持续时间
    myIMU.writeRegister(LSM6DS3_ACC_GYRO_WAKE_UP_DUR, 0x00);
    
    // 6. Route wake-up to INT1
    // 6. 将唤醒路由到 INT1
    myIMU.writeRegister(LSM6DS3_ACC_GYRO_MD1_CFG, 0x20);
    
    // Clear interrupt
    // 清除中断
    uint8_t reg;
    myIMU.readRegister(&reg, LSM6DS3_ACC_GYRO_WAKE_UP_SRC);
}

// =============================================================================
// Deep Sleep Function | 深度睡眠函数
// =============================================================================

/**
 * Enter System OFF deep sleep mode
 * Uses SoftDevice API for proper shutdown
 * 进入 System OFF 深度睡眠模式
 * 使用 SoftDevice API 进行正确关闭
 */
void goToSleep() {
    Serial.println();
    Serial.println(">>> Entering deep sleep <<<");
    Serial.flush();
    
    // Stop BLE advertising
    // 停止 BLE 广播
    Bluefruit.Advertising.stop();
    delay(100);
    
    ledsOff();
    
    // Blue LED blinks 3 times to indicate sleep
    // 蓝色 LED 闪烁 3 次表示即将睡眠
    for (int i = 0; i < 3; i++) {
        setBlueLed(true);
        delay(200);
        setBlueLed(false);
        delay(200);
    }
    
    // Configure IMU wake-up interrupt
    // 配置 IMU 唤醒中断
    setupWakeUpInterrupt();
    delay(100);
    
    // Clear any pending interrupt
    // 清除任何待处理的中断
    uint8_t dummy;
    myIMU.readRegister(&dummy, LSM6DS3_ACC_GYRO_WAKE_UP_SRC);
    
    Serial.println("Waiting 3 seconds before sleep...");
    Serial.flush();
    delay(3000);
    
    // Clear interrupt again
    // 再次清除中断
    myIMU.readRegister(&dummy, LSM6DS3_ACC_GYRO_WAKE_UP_SRC);
    
    // Shutdown I2C
    // 关闭 I2C
    Wire.end();
    
    // Shutdown Serial
    // 关闭串口
    Serial.end();
    
    // Detach Arduino interrupt
    // 断开 Arduino 中断
    detachInterrupt(digitalPinToInterrupt(IMU_INT1_PIN));
    
    // Configure wake-up pin using SoftDevice API
    // 使用 SoftDevice API 配置唤醒引脚
    nrf_gpio_cfg_sense_input(IMU_INT1_PIN, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
    
    // Set LED pins HIGH (off)
    // 设置 LED 引脚为高电平（关闭）
    nrf_gpio_cfg_output(LED_GREEN_PIN);
    nrf_gpio_cfg_output(LED_BLUE_PIN);
    nrf_gpio_pin_set(LED_GREEN_PIN);
    nrf_gpio_pin_set(LED_BLUE_PIN);
    
    delay(100);
    
    // Enter System OFF using SoftDevice
    // 使用 SoftDevice 进入 System OFF
    sd_power_system_off();
    
    // Code never reaches here
    // 代码永远不会执行到这里
    while(1) { __WFE(); }
}

// =============================================================================
// Arduino Main Program | Arduino 主程序
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  XIAO nRF52840 BTHome Motion Detect");
    Serial.println("========================================");
    Serial.println();

    // =========================================================================
    // Check wake-up reason | 检查唤醒原因
    // =========================================================================
    
    // Read reset reason register
    // 读取复位原因寄存器
    uint32_t resetReason = NRF_POWER->RESETREAS;
    bool wokeFromSleep = (resetReason & 0x10000);  // Bit 16: GPIO wake from System OFF
                                                    // Bit 16: 从 System OFF GPIO 唤醒
    
    // Clear reset reason (write 1 to clear)
    // 清除复位原因（写 1 清除）
    NRF_POWER->RESETREAS = resetReason;
    
    Serial.print("Reset reason: 0x");
    Serial.println(resetReason, HEX);
    Serial.println(wokeFromSleep ? ">>> Woke from sleep (motion triggered) <<<" : ">>> Normal power-on <<<");
    Serial.println();

    // Enable DC-DC converter for efficiency
    // 启用 DC-DC 转换器以提高效率
    NRF_POWER->DCDCEN = 1;

    // =========================================================================
    // Initialize LEDs | 初始化 LED
    // =========================================================================
    
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);
    ledsOff();

    // Green LED blink to indicate startup (fast for wake, slow for power-on)
    // 绿色 LED 闪烁表示启动（唤醒时快闪，上电时慢闪）
    int blinkCount = wokeFromSleep ? 2 : 3;
    int blinkDelay = wokeFromSleep ? 100 : 150;
    for (int i = 0; i < blinkCount; i++) {
        setGreenLed(true);
        delay(blinkDelay);
        setGreenLed(false);
        delay(blinkDelay);
    }

    // =========================================================================
    // Initialize BLE and get MAC address | 初始化 BLE 并获取 MAC 地址
    // =========================================================================
    
    Bluefruit.begin();
    uint8_t mac[6];
    Bluefruit.getAddr(mac);
    
    // Format MAC address string (AA:BB:CC:DD:EE:FF)
    // 格式化 MAC 地址字符串
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    
    // Device name uses last 4 digits of MAC
    // 设备名称使用 MAC 后 4 位
    sprintf(deviceName, "SeeedUA-%02X%02X", mac[1], mac[0]);
    
    Serial.print("MAC Address: ");
    Serial.println(macStr);
    Serial.print("Device Name: ");
    Serial.println(deviceName);
    Serial.println();

    // =========================================================================
    // Configure IMU interrupt | 配置 IMU 中断
    // =========================================================================
    
    pinMode(IMU_INT1_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(IMU_INT1_PIN), int1ISR, RISING);

    // =========================================================================
    // Initialize IMU | 初始化 IMU
    // =========================================================================
    
    if (myIMU.begin() != 0) {
        Serial.println("IMU initialization failed!");
        // Blue LED continuous blink indicates error
        // 蓝色 LED 持续闪烁表示错误
        while (1) {
            setBlueLed(true);
            delay(500);
            setBlueLed(false);
            delay(500);
        }
    }
    Serial.println("IMU initialization successful!");

    // =========================================================================
    // Configure BLE advertising | 配置 BLE 广播
    // =========================================================================
    
    Bluefruit.setTxPower(4);
    Bluefruit.setName(deviceName);
    
    // Build initial BTHome Service Data (motion=1)
    // 构建初始 BTHome 服务数据（motion=1）
    uint8_t serviceData[8];
    uint8_t len = 0;
    serviceData[len++] = 0xD2;  // UUID 0xFCD2 (little-endian)
    serviceData[len++] = 0xFC;
    serviceData[len++] = BTHOME_DEVICE_INFO;
    serviceData[len++] = BTHOME_BINARY_MOTION;
    serviceData[len++] = 0x01;  // Motion detected
                                // 检测到运动
    
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, serviceData, len);
    Bluefruit.Advertising.setInterval(160, 160);

    // =========================================================================
    // Start based on wake-up reason | 根据唤醒原因启动
    // =========================================================================
    
    if (wokeFromSleep) {
        // Woke from sleep = motion detected, start advertising immediately
        // 从睡眠唤醒 = 检测到运动，立即开始广播
        Serial.println("Motion wake! Starting BTHome advertising...");
        
        // Send motion=1 multiple times to ensure HA receives it
        // 多次发送 motion=1 确保 HA 收到
        for (int i = 0; i < 5; i++) {
            Bluefruit.Advertising.stop();
            Bluefruit.Advertising.clearData();
            Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
            Bluefruit.Advertising.addName();
            Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, serviceData, len);
            Bluefruit.Advertising.setInterval(32, 32);  // Fast advertising
                                                         // 快速广播
            Bluefruit.Advertising.start(0);
            delay(200);
            Serial.print(".");
        }
        Serial.println(" OK");
        
        // Restore normal advertising interval
        // 恢复正常广播间隔
        Bluefruit.Advertising.setInterval(160, 160);
        
        bleStarted = true;
        isMotionState = true;
        lastAdvertiseTime = millis();
        
        setGreenLed(true);
        delay(100);
        setGreenLed(false);
    } else {
        // Normal power-on, also start advertising
        // 正常上电，也开始广播
        Serial.println("Normal power-on, starting advertising...");
        Bluefruit.Advertising.start(0);
        bleStarted = true;
        isMotionState = true;
        lastAdvertiseTime = millis();
    }

    // =========================================================================
    // Initialization complete | 初始化完成
    // =========================================================================

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Initialization Complete!");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Motion: broadcast motion=1");
    Serial.println("Still 6s: switch to motion=0");
    Serial.println("Still 11s: enter deep sleep");
    Serial.println();
    Serial.println("IMPORTANT: Disconnect USB to test power consumption!");
    Serial.println();
    
    lastMotionTime = millis();
    lastAdvertiseTime = millis();
}

void loop() {
    // Logic:
    // 逻辑：
    // - Motion detected: broadcast motion=1 at high frequency
    //   检测到运动：高频广播 motion=1
    // - Still for 6 seconds: switch to motion=0
    //   静止 6 秒：切换到 motion=0
    // - Still for 11 seconds total: enter deep sleep
    //   总共静止 11 秒：进入深度睡眠
    
    float zAccel = myIMU.readFloatAccelZ();
    bool hasMotion = (zAccel > ACCEL_THRESHOLD_Z);
    unsigned long now = millis();
    
    if (hasMotion) {
        // Motion detected
        // 检测到运动
        lastMotionTime = now;
        lastStillTime = 0;  // Reset still timer | 重置静止计时
        
        // If currently in clear state, switch back to motion state
        // 如果当前是静止状态，切换回运动状态
        if (!isMotionState) {
            isMotionState = true;
            Serial.println(">>> Motion=1");
            setGreenLed(true);
            delay(30);
            setGreenLed(false);
        }
        
        // During motion, refresh advertising at high frequency
        // 运动期间，高频刷新广播
        if (now - lastAdvertiseTime > ADVERTISE_INTERVAL) {
            updateAdvertising(true);
            lastAdvertiseTime = now;
        }
    } else {
        // No motion
        // 没有运动
        if (lastStillTime == 0) {
            lastStillTime = now;
        }
        
        // Still for more than 6 seconds, switch to clear state
        // 静止超过 6 秒，切换到静止状态
        if (isMotionState && (now - lastStillTime > DEBOUNCE_TIME)) {
            isMotionState = false;
            Serial.println(">>> Motion=0, sleep in 5s");
        }
        
        // During still period, also refresh advertising
        // 静止期间也刷新广播
        if (now - lastAdvertiseTime > ADVERTISE_INTERVAL) {
            updateAdvertising(isMotionState);
            lastAdvertiseTime = now;
        }
        
        // After switching to clear and 5 more seconds, enter sleep
        // 切换到静止后再过 5 秒，进入睡眠
        if (!isMotionState && (now - lastMotionTime > CLEAR_DURATION + DEBOUNCE_TIME)) {
            Serial.println(">>> Entering sleep");
            goToSleep();
        }
    }
    
    delay(20);  // 20ms detection interval | 20ms 检测间隔
}
