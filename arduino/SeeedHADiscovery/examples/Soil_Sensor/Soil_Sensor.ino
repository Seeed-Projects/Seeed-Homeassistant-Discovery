/**
 * ============================================================================
 * Seeed HA Discovery - Soil Moisture Sensor Example
 * Seeed HA Discovery - 土壤湿度传感器示例
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Connect ESP32 to WiFi
 *    将 ESP32 连接到 WiFi
 * 2. Create soil moisture sensor
 *    创建土壤湿度传感器
 * 3. Report sensor data to Home Assistant in real-time
 *    向 Home Assistant 实时上报传感器数据
 *
 * Hardware Requirements:
 * - XIAO ESP32-C3/C5/C6/S3 or other ESP32 development boards
 *   XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
 * - Note: XIAO ESP32-C5 supports both 2.4GHz and 5GHz WiFi
 *   Note: XIAO ESP32-C5 supports both 2.4GHz and 5GHz WiFi
 * -  Soil Moisture Sensor (required for actual data collection)
 *  土壤湿度传感器（实际数据采集需使用）
 *Soil Moisture Sensor Wiring:
 * 土壤湿度传感器接线说明：
 * - VCC  -> 3.3V
 * - GND  -> GND
 * - EXCITE -> D3 (configurable below)
 *   激励引脚 -> D3 (可在下方修改)
 * - SAMPLE -> D1 (configurable below)
 *   采样引脚 -> D1 (可在下方修改)
 *
 * LED Wiring:
 * LED接线说明：
 * - GREEN LED -> D8
 *   绿灯 -> D8
 * - RED LED -> D9
 *   红灯 -> D9
 * - YELLOW LED -> D10
 *   黄灯 -> D10
 *
 * Software Dependencies:
 * - ArduinoJson (by Benoit Blanchon)
 * - WebSockets (by Markus Sattler)
 *
 * Usage:
 * 使用方法：
 * 1. Modify WiFi configuration below
 *    修改下方的 WiFi 配置
 * 2. Upload to ESP32
 *    上传到 ESP32
 * 3. Open Serial Monitor to view IP address
 *    打开串口监视器查看 IP 地址
 * 4. Add device in Home Assistant
 *    在 Home Assistant 中添加设备
 *
 * @author limengdu
 * @version 1.2.0
 */

#include <SeeedHADiscovery.h>


// =============================================================================
// Configuration - Please modify according to your environment
// 配置区域 - 请根据你的环境修改
// =============================================================================

// WiFi Provisioning Configuration | WiFi 配网配置
// Set to true to enable web-based WiFi provisioning (recommended)
// Set to false to use hardcoded credentials below
// 设置为 true 启用网页配网（推荐）
// 设置为 false 使用下面的硬编码凭据
#define USE_WIFI_PROVISIONING true

// AP hotspot name for WiFi provisioning | 配网时的 AP 热点名称
const char* AP_SSID = "Seeed_SoilSensor_AP";

// Fallback WiFi credentials (only used if USE_WIFI_PROVISIONING is false)
// 备用 WiFi 凭据（仅在 USE_WIFI_PROVISIONING 为 false 时使用）
const char* WIFI_SSID = "Your_WiFi_SSID";      // Your WiFi SSID | 你的WiFi名称
const char* WIFI_PASSWORD = "Your_WiFi_Password";  // Your WiFi password | 你的WiFi密码

const unsigned long UPDATE_INTERVAL = 5000;  // Data report interval | 上报间隔保留
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

// ==========  Soil  Sensor Configuration ==========
// ========== 土壤湿度传感器配置 ==========
// Pin Definition (strictly match schematic)
// 引脚定义（严格对应原理图）
#define SOIL_EXCITE_PIN  D3   // Excitation signal pin (A3_D3) | 激励信号引脚（A3_D3）
#define SOIL_SAMPLE_PIN  D1   // Sampling pin (A1_D1) | 采样引脚（A1_D1）

#define LED_GREEN_PIN    D8    // Green LED (A8_D8) | 绿灯（A8_D8）
#define LED_RED_PIN      D9    // Red LED (A9_D9) | 红灯（A9_D9）
#define LED_YELLOW_PIN   D10   // Yellow LED (A10_D10) | 黄灯（A10_D10）

// 【Key】Replace with your actual calibrated values!
// 【关键】替换为你实测的校准值！
const int ACTUAL_DRY_ADC = 5;     // Dry state ADC value (measured by you) | 干燥状态ADC值（你实测的5）
const int ACTUAL_WET_ADC = 66;    // Wet state ADC value (measured by you) | 湿润状态ADC值（你实测的66）

// 3. Moisture level thresholds (adjustable as needed)
// 3. 湿度分级阈值（可根据需求调整）
const int DRY_THRESHOLD = 30;     // Dry threshold (≤30%) | 干燥阈值（≤30%）
const int WET_THRESHOLD = 70;     // Wet threshold (≥70%) | 湿润阈值（≥70%）

// Enumeration: Easy to distinguish moisture levels (optional, improve readability)
// 枚举类型：方便区分湿度等级（可选，增强代码可读性）
typedef enum {
  DRY,        // Dry | 干燥
  NORMAL,     // Normal | 普通
  WET         // Wet | 湿润
} MoistureLevel;




// =============================================================================
// Global Variables | 全局变量
// =============================================================================

SeeedHADiscovery ha;
SeeedHASensor* soilMoistureSensor;

// =============================================================================
// Helper Functions | 辅助函数
// =============================================================================
// Helper function: Judge level by moisture value
// 辅助函数：根据湿度值判断等级
MoistureLevel getMoistureLevel(float moisture) {
  if (moisture <= DRY_THRESHOLD) {
    return DRY;
  } else if (moisture >= WET_THRESHOLD) {
    return WET;
  } else {
    return NORMAL;
  }
}
// Helper function: Control LED on/off (only one light on at a time)
// 辅助函数：控制LED亮灭（同一时间只亮一盏）
void controlLED(MoistureLevel level) {
  // Turn off all lights first (avoid multiple lights on at the same time)
  // 先灭所有灯（避免多灯同时亮）
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);

  // Turn on corresponding light according to level
  // 根据等级亮对应灯
  switch (level) {
    case DRY:
      digitalWrite(LED_RED_PIN, HIGH);
      Serial.println("Status: Dry → Red LED On");
      break;
    case NORMAL:
      digitalWrite(LED_YELLOW_PIN, HIGH);
      Serial.println("Status: Normal → Yellow LED On");
      break;
    case WET:
      digitalWrite(LED_GREEN_PIN, HIGH);
      Serial.println("Status: Wet → Green LED On");
      break;
  }
}


// =============================================================================
// Arduino Main Program | Arduino 主程序
// =============================================================================

void setup() {
    // Initialize serial | 初始化串口
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Seeed HA Discovery - Soil Sensor");
    Serial.println("========================================");
    Serial.println();

  // Initialize pin modes
  // 初始化引脚模式
  pinMode(SOIL_EXCITE_PIN, OUTPUT);  // D3 set as output (excitation signal) | D3设为输出（激励信号）
  pinMode(SOIL_SAMPLE_PIN, INPUT);   // D1 set as input (ADC sampling) | D1设为输入（ADC采样）

    // Initialize LED pins (all set as output, off initially)
    // 初始化LED引脚（全部设为输出，初始灭灯）
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);

    // Configure device info | 配置设备信息
    ha.setDeviceInfo(
        "Soil Moisture Sensor", // Device name | 设备名称
        "XIAO ESP32",          // Device model | 设备型号
        "1.0.0"                // Firmware version | 固件版本
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

    // Connect WiFi | 连接 WiFi
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
    Serial.println("Connecting to WiFi...");
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi connection failed!");
        while (1) delay(1000);
    }
#endif

    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(ha.getLocalIP().toString().c_str());

    // =========================================================================
    // Create sensors | 创建传感器
    // =========================================================================

      // New: Register HA soil moisture sensor
      // 新增：注册HA土壤湿度传感器
     soilMoistureSensor = ha.addSensor(
        "soil_moisture",    // Sensor unique ID | 传感器唯一标识
        "Soil Moisture",    // Display name in HA | HA中显示的名称
        "humidity",         // Sensor type (humidity) | 传感器类型（湿度类）
        "%"                 // Unit | 单位
    );
    soilMoistureSensor->setPrecision(1);  // Keep 1 decimal place | 保留1位小数

 // Print configuration information
 // 打印配置信息
  Serial.print("Excitation Pin: D3 (GPIO");
  Serial.print(SOIL_EXCITE_PIN);
  Serial.println(") [Output]");
  Serial.print("Sampling Pin: D1 (GPIO");
  Serial.print(SOIL_SAMPLE_PIN);
  Serial.println(") [Input]");
  Serial.println("LED Pins: D8(Green)、D9(Red)、D10(Yellow)");
  Serial.println("------------------------");
  Serial.print("Dry Threshold: ≤");
  Serial.print(DRY_THRESHOLD);
  Serial.println("% (Red LED)");
  Serial.print("Normal Range: ");
  Serial.print(DRY_THRESHOLD);
  Serial.print("% ~ ");
  Serial.print(WET_THRESHOLD);
  Serial.println("% (Yellow LED)");
  Serial.print("Wet Threshold: ≥");
  Serial.print(WET_THRESHOLD);
  Serial.println("% (Green LED)");
  Serial.println("------------------------");
}

void loop() {
    // Must call! Handle network events
    // 必须调用！处理网络事件
    ha.handle();

#if USE_WIFI_PROVISIONING
    // If in provisioning mode, only handle AP | 如果处于配网模式，只处理 AP
    if (ha.isProvisioningActive()) {
        delay(10);
        return;
    }
#endif

    // Periodically read and report sensor data
    // 定期读取并上报传感器数据
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > UPDATE_INTERVAL) {
        lastUpdate = millis();

  // 1. Output excitation signal (trigger logic required by schematic)
  // 1. 输出激励信号（原理图要求的触发逻辑）
  digitalWrite(SOIL_EXCITE_PIN, HIGH);  // D3 output high level | D3输出高电平
  delayMicroseconds(10);                // Hold for 10 microseconds (match circuit RC parameters) | 保持10微秒（匹配电路RC参数）
  digitalWrite(SOIL_EXCITE_PIN, LOW);   // D3 output low level | D3输出低电平
  delayMicroseconds(50);                // Wait for signal stabilization | 等待信号稳定

  // 2. Read ADC raw value (ESP32-C6 default 12-bit ADC, range 0~4095)
  // 2. 读取ADC原始值（ESP32-C6默认12位ADC，范围0~4095）
  int adcValue = analogRead(SOIL_SAMPLE_PIN);

  // 3. Correction: Reverse mapping percentage (adapt to your sensor rules)
  // 3. 修正：反向映射百分比（适配你的传感器规律）
  // Logic: The closer ADC value to ACTUAL_WET_ADC (66), the higher humidity; closer to ACTUAL_DRY_ADC (5), the lower humidity
  // 逻辑：ADC值越接近ACTUAL_WET_ADC（66），湿度越高；越接近ACTUAL_DRY_ADC（5），湿度越低
  float moisturePercent = map(adcValue, ACTUAL_DRY_ADC, ACTUAL_WET_ADC, 0, 100);
  // Limit range to avoid abnormal values
  // 限制范围，避免异常值
  moisturePercent = constrain(moisturePercent, 0, 100);

  // 4. Serial print data (for debugging)
  // 4. 串口打印数据（方便调试）
  Serial.print("ADC Raw Value: ");
  Serial.print(adcValue);
  Serial.print(" | Soil Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  // 5. Judge moisture level and control LED
  // 5. 判断湿度等级并控制LED
  MoistureLevel level = getMoistureLevel(moisturePercent);
  controlLED(level);
    // New: Report soil moisture data to HA (core missing logic)
    // 新增：上报土壤湿度数据到HA（核心缺失逻辑）
        soilMoistureSensor->setValue(moisturePercent);
        Serial.print("Reported to HA: Soil Moisture = ");
        Serial.print(moisturePercent, 1);
        Serial.println("%");
        // ========== Soil moisture logic end ==========

    // Connection status monitoring | 连接状态监控
    static unsigned long lastCheck = 0;
    static bool wasConnected = false;

    if (millis() - lastCheck > 5000) {
        lastCheck = millis();

        bool connected = ha.isHAConnected();
        if (connected != wasConnected) {
            Serial.println(connected ? "HA Connected" : "HA Disconnected");
            wasConnected = connected;
        }
    }

      // 5. Sampling interval (once every 2 seconds, avoid frequent reading)
      // 5. 采样间隔（2秒一次，避免频繁读取）
  delay(100);
}
}