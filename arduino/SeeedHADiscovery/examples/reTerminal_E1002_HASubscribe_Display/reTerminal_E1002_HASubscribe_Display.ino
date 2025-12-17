/**
 * ============================================================================
 * reTerminal E1002 - HA State Subscribe with E-Paper Display (6-Color)
 * reTerminal E1002 - HA 状态订阅墨水屏显示示例（六色）
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Web-based WiFi provisioning (captive portal)
 *    网页配网（强制门户）
 * 2. Subscribe to Home Assistant entity states
 *    订阅 Home Assistant 实体状态
 * 3. Display the states on a 6-color E-Paper display (800x480)
 *    在六色墨水屏上显示状态（800x480）
 * 4. Create a beautiful colorful dashboard UI
 *    创建美观的彩色仪表板界面
 *
 * Hardware:
 * 硬件：
 * - reTerminal E1002 with 6-color E-Paper display
 *   reTerminal E1002 带六色墨水屏
 * - Display resolution: 800x480
 *   显示分辨率：800x480
 * - GPIO3: Reset button (long press 6s to reset WiFi)
 *   GPIO3：重置按钮（长按6秒重置WiFi）
 * - GPIO6: Status LED (active LOW, provides visual feedback)
 *   GPIO6：状态 LED（低电平点亮，提供视觉反馈）
 *
 * Supported Colors (6 colors only):
 * 支持的颜色（仅6色）：
 * - TFT_WHITE  (白色)
 * - TFT_BLACK  (黑色)
 * - TFT_RED    (红色)
 * - TFT_GREEN  (绿色)
 * - TFT_BLUE   (蓝色)
 * - TFT_YELLOW (黄色)
 *
 * Dependencies:
 * 依赖库：
 * - Seeed_GFX: https://github.com/Seeed-Studio/Seeed_GFX
 * - SeeedHADiscovery
 *
 * WiFi Provisioning:
 * WiFi 配网：
 * - On first boot (no saved credentials), device creates AP
 *   首次启动（无保存凭据）时，设备创建 AP
 * - Connect to AP and open http://192.168.4.1 in browser
 *   连接到 AP 并在浏览器中打开 http://192.168.4.1
 * - Select WiFi network and enter password
 *   选择 WiFi 网络并输入密码
 * - Credentials are saved and used on subsequent boots
 *   凭据被保存并在后续启动时使用
 * - Long press GPIO3 (6+ seconds) to clear credentials and restart
 *   长按 GPIO3（6秒以上）清除凭据并重启
 *
 * Setup Steps:
 * 设置步骤：
 * 1. Upload to reTerminal E1002
 *    上传到 reTerminal E1002
 * 2. Configure WiFi via captive portal (or use hardcoded credentials)
 *    通过强制门户配置 WiFi（或使用硬编码凭据）
 * 3. In Home Assistant, find your device and click "Configure"
 *    在 Home Assistant 中找到设备并点击"配置"
 * 4. Subscribe to entities (e.g., temperature, humidity sensors)
 *    订阅实体（如温度、湿度传感器）
 * 5. The display will update automatically
 *    显示屏将自动更新
 *
 * @author Seeed Studio
 * @version 1.1.0
 */

#include <SeeedHADiscovery.h>
#include "TFT_eSPI.h"

// =============================================================================
// Configuration | 配置
// =============================================================================

// WiFi Provisioning Configuration | WiFi 配网配置
// Set to true to enable web-based WiFi provisioning (recommended)
// Set to false to use hardcoded credentials below
// 设置为 true 启用网页配网（推荐）
// 设置为 false 使用下面的硬编码凭据
#define USE_WIFI_PROVISIONING true

// AP hotspot name for WiFi provisioning | 配网时的 AP 热点名称
const char* AP_SSID = "reTerminal_E1002_AP";

// Fallback WiFi credentials (only used if USE_WIFI_PROVISIONING is false)
// 备用 WiFi 凭据（仅在 USE_WIFI_PROVISIONING 为 false 时使用）
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";

// Reset button pin (long press 6s to reset WiFi) | 重置按钮引脚（长按6秒重置WiFi）
#define PIN_RESET_BUTTON 3

// Status LED pin (active LOW) | 状态 LED 引脚（低电平点亮）
#define PIN_STATUS_LED 6

// Buzzer pin | 蜂鸣器引脚
#define PIN_BUZZER 45

// WiFi reset hold time (ms) | WiFi 重置按住时间（毫秒）
#define WIFI_RESET_HOLD_TIME 6000

// Display refresh interval (ms) - E-Paper refresh is slow, don't update too often
// 显示刷新间隔（毫秒）- 墨水屏刷新较慢，不要太频繁
const unsigned long DISPLAY_REFRESH_INTERVAL = 300000;  // 5 minutes | 5分钟

// Serial1 pin configuration for reTerminal E1002
// reTerminal E1002 的 Serial1 引脚配置
#define SERIAL_RX 44
#define SERIAL_TX 43

// =============================================================================
// Display Configuration | 显示配置
// =============================================================================

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

// Screen dimensions | 屏幕尺寸
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 480;

// Layout constants | 布局常量
const int HEADER_HEIGHT = 60;
const int CARD_WIDTH = 240;
const int CARD_HEIGHT = 140;
const int CARD_MARGIN = 20;
const int CARD_PADDING = 15;

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

SeeedHADiscovery ha;
unsigned long lastDisplayUpdate = 0;
bool initialRefreshDone = false;  // Track if initial refresh is done | 跟踪初始刷新是否完成

// Wait time after receiving data before refresh (to collect all entities)
// 收到数据后等待一段时间再刷新（以便收集所有实体）
const unsigned long DATA_COLLECTION_WAIT = 5000;  // 5 seconds | 5秒
unsigned long dataReceivedTime = 0;  // Time when first data was received | 收到第一批数据的时间
bool dataReceived = false;  // Flag to track if data has been received | 标记是否已收到数据
int lastEntityCount = 0;  // Track entity count changes | 跟踪实体数量变化

// Config change detection | 配置变更检测
bool configChanged = false;  // Flag set when HA clears states (config update) | 当 HA 清除状态时设置（配置更新）
unsigned long configChangeTime = 0;  // Time when config change was detected | 检测到配置变更的时间

// HA connection status tracking | HA 连接状态跟踪
bool lastHAConnected = false;  // Previous HA connection state | 上次 HA 连接状态
unsigned long haStatusChangeTime = 0;  // Time when HA status changed | HA 状态变化时间
bool haStatusPendingRefresh = false;  // Flag for pending refresh due to HA status change | HA 状态变化待刷新标志
const unsigned long HA_STATUS_DEBOUNCE = 30000;  // 30 seconds debounce for HA status | HA 状态防抖时间30秒

// WiFi provisioning mode tracking | WiFi 配网模式跟踪
bool wifiProvisioningMode = false;

// Reset button state tracking | 重置按钮状态跟踪
volatile uint32_t resetButtonPressTime = 0;
volatile bool resetButtonPressed = false;
volatile bool resetFeedbackGiven = false;
volatile bool wifiResetRequested = false;  // Flag to trigger reset from main loop | 主循环触发重置的标志

// RTOS task handle for reset button | 重置按钮的 RTOS 任务句柄
TaskHandle_t resetButtonTaskHandle = NULL;

// Entity data structure | 实体数据结构
struct EntityDisplay {
    String entityId;
    String friendlyName;
    String state;
    String unit;
    uint16_t color;
    bool hasValue;
};

// Maximum entities to display | 最大显示实体数
const int MAX_DISPLAY_ENTITIES = 6;
EntityDisplay displayEntities[MAX_DISPLAY_ENTITIES];
int entityCount = 0;

// =============================================================================
// LED Control Functions | LED 控制函数
// =============================================================================

/**
 * Set status LED state | 设置状态 LED 状态
 * Active LOW: LOW = ON, HIGH = OFF
 * 低电平有效：LOW = 点亮，HIGH = 熄灭
 */
void setStatusLED(bool on) {
    digitalWrite(PIN_STATUS_LED, on ? LOW : HIGH);
}

/**
 * Reset button detection task (runs on Core 0)
 * 重置按钮检测任务（运行在 Core 0）
 * 
 * This task runs independently on Core 0, monitoring the reset button
 * without blocking the main loop (which handles WiFi/HTTP on Core 1).
 * 此任务独立运行在 Core 0，监控重置按钮，
 * 不会阻塞主循环（主循环在 Core 1 处理 WiFi/HTTP）。
 */
void resetButtonTask(void* parameter) {
    Serial1.println("[RTOS] Reset button task started on Core " + String(xPortGetCoreID()));
    
    while (true) {
        bool currentState = (digitalRead(PIN_RESET_BUTTON) == LOW);
        uint32_t now = millis();
        
        // Button just pressed | 按钮刚被按下
        if (currentState && !resetButtonPressed) {
            resetButtonPressed = true;
            resetButtonPressTime = now;
            resetFeedbackGiven = false;
            Serial1.println("Reset button pressed...");
        }
        
        // Button held - check for 6 second threshold | 按钮保持按下 - 检查6秒阈值
        if (currentState && resetButtonPressed && !resetFeedbackGiven) {
            uint32_t holdTime = now - resetButtonPressTime;
            
            if (holdTime >= WIFI_RESET_HOLD_TIME) {
                resetFeedbackGiven = true;
                Serial1.println();
                Serial1.println("=========================================");
                Serial1.println("  WiFi Reset threshold reached (6s)!");
                Serial1.println("  Release button to reset WiFi...");
                Serial1.println("=========================================");
                
                // Audio + Visual feedback (on Core 0, won't block main loop)
                // 声音 + 视觉反馈（在 Core 0，不会阻塞主循环）
                for (int i = 0; i < 3; i++) {
                    tone(PIN_BUZZER, 1500);
                    setStatusLED(true);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    noTone(PIN_BUZZER);
                    tone(PIN_BUZZER, 1000);
                    setStatusLED(false);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    noTone(PIN_BUZZER);
                }
                setStatusLED(true);
                tone(PIN_BUZZER, 2000);
                vTaskDelay(pdMS_TO_TICKS(200));
                noTone(PIN_BUZZER);
            }
        }
        
        // Button released | 按钮释放
        if (!currentState && resetButtonPressed) {
            uint32_t holdTime = now - resetButtonPressTime;
            resetButtonPressed = false;
            
            // If held long enough, set flag for main loop to handle reset
            // 如果按住足够长，设置标志让主循环处理重置
            if (resetFeedbackGiven && holdTime >= WIFI_RESET_HOLD_TIME) {
                Serial1.println();
                Serial1.println("=========================================");
                Serial1.println("  WiFi Reset triggered!");
                Serial1.println("=========================================");
                
                tone(PIN_BUZZER, 800);
                vTaskDelay(pdMS_TO_TICKS(500));
                noTone(PIN_BUZZER);
                setStatusLED(false);
                
                // Set flag for main loop (WiFi operations should be on main core)
                // 设置标志给主循环（WiFi 操作应在主核心执行）
                wifiResetRequested = true;
            }
            
            setStatusLED(false);
            resetFeedbackGiven = false;
        }
        
        // Small delay to prevent hogging CPU | 小延时防止占用 CPU
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// =============================================================================
// UI Drawing Functions | UI 绘制函数
// =============================================================================

/**
 * Get color based on device class
 * 根据设备类别获取颜色
 */
uint16_t getColorForDeviceClass(const String& deviceClass) {
    if (deviceClass == "temperature") {
        return TFT_RED;
    } else if (deviceClass == "humidity") {
        return TFT_BLUE;
    } else if (deviceClass == "battery") {
        return TFT_GREEN;
    } else if (deviceClass == "illuminance") {
        return TFT_YELLOW;
    } else if (deviceClass == "power" || deviceClass == "energy") {
        return TFT_GREEN;
    } else {
        return TFT_BLACK;
    }
}

/**
 * Draw header bar
 * 绘制顶部标题栏
 */
void drawHeader() {
#ifdef EPAPER_ENABLE
    // Header background | 标题栏背景
    epaper.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_BLUE);
    
    // Title text | 标题文字
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("Home Assistant Dashboard", 20, 18);
    
    // Connection status | 连接状态
    String status = ha.isHAConnected() ? "Connected" : "Waiting...";
    epaper.setTextSize(2);
    epaper.drawString(status, SCREEN_WIDTH - 150, 22);
    
    // Status indicator circle | 状态指示圆点
    uint16_t statusColor = ha.isHAConnected() ? TFT_GREEN : TFT_YELLOW;
    epaper.fillCircle(SCREEN_WIDTH - 170, 30, 8, statusColor);
#endif
}

/**
 * Draw a single entity card
 * 绘制单个实体卡片
 */
void drawEntityCard(int x, int y, EntityDisplay& entity) {
#ifdef EPAPER_ENABLE
    // Card background (white) | 卡片背景（白色）
    epaper.fillRect(x, y, CARD_WIDTH, CARD_HEIGHT, TFT_WHITE);
    
    // Card border | 卡片边框
    epaper.drawRect(x, y, CARD_WIDTH, CARD_HEIGHT, TFT_BLACK);
    epaper.drawRect(x + 1, y + 1, CARD_WIDTH - 2, CARD_HEIGHT - 2, TFT_BLACK);
    
    // Color accent bar on left | 左侧颜色条
    epaper.fillRect(x, y, 8, CARD_HEIGHT, entity.color);
    
    // Entity name | 实体名称
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(2);
    
    String displayName = entity.friendlyName;
    if (displayName.length() == 0) {
        displayName = entity.entityId;
    }
    // Truncate long names | 截断过长的名称
    if (displayName.length() > 18) {
        displayName = displayName.substring(0, 15) + "...";
    }
    epaper.drawString(displayName, x + CARD_PADDING + 5, y + CARD_PADDING);
    
    // Value | 数值
    if (entity.hasValue) {
        epaper.setTextColor(entity.color);
        epaper.setTextSize(4);
        
        String valueStr = entity.state;
        // Truncate decimals if too long | 如果太长则截断小数
        if (valueStr.length() > 6) {
            float val = valueStr.toFloat();
            valueStr = String(val, 1);
        }
        
        epaper.drawString(valueStr, x + CARD_PADDING + 5, y + 50);
        
        // Unit | 单位
        if (entity.unit.length() > 0) {
            epaper.setTextSize(2);
            epaper.setTextColor(TFT_BLACK);
            int valueWidth = valueStr.length() * 24;  // Approximate width
            epaper.drawString(entity.unit, x + CARD_PADDING + 10 + valueWidth, y + 65);
        }
    } else {
        // Waiting for data | 等待数据
        epaper.setTextColor(TFT_BLACK);
        epaper.setTextSize(2);
        epaper.drawString("Waiting...", x + CARD_PADDING + 5, y + 60);
    }
    
    // Last update indicator (small dot) | 最后更新指示（小圆点）
    epaper.fillCircle(x + CARD_WIDTH - 15, y + CARD_HEIGHT - 15, 5, 
                      entity.hasValue ? TFT_GREEN : TFT_YELLOW);
#endif
}

/**
 * Draw empty card placeholder
 * 绘制空卡片占位符
 */
void drawEmptyCard(int x, int y) {
#ifdef EPAPER_ENABLE
    // Dashed border effect | 虚线边框效果
    epaper.drawRect(x, y, CARD_WIDTH, CARD_HEIGHT, TFT_BLACK);
    
    // Plus sign | 加号
    int centerX = x + CARD_WIDTH / 2;
    int centerY = y + CARD_HEIGHT / 2;
    
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(3);
    epaper.drawString("+", centerX - 10, centerY - 15);
    
    // Hint text | 提示文字
    epaper.setTextSize(1);
    epaper.drawString("Subscribe in HA", x + 60, y + CARD_HEIGHT - 25);
#endif
}

/**
 * Draw footer with device info
 * 绘制底部设备信息
 */
void drawFooter() {
#ifdef EPAPER_ENABLE
    int footerY = SCREEN_HEIGHT - 35;
    
    // Footer line | 底部分隔线
    epaper.drawLine(20, footerY - 5, SCREEN_WIDTH - 20, footerY - 5, TFT_BLACK);
    
    // Device info | 设备信息
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(1);
    
    String deviceInfo = "Device: " + ha.getDeviceId() + " | IP: " + ha.getLocalIP().toString();
    epaper.drawString(deviceInfo, 20, footerY + 5);
    
    // Update time | 更新时间
    String updateInfo = "Last refresh: " + String(millis() / 1000) + "s ago";
    epaper.drawString(updateInfo, SCREEN_WIDTH - 200, footerY + 5);
#endif
}

/**
 * Draw the complete dashboard
 * 绘制完整仪表板
 */
void drawDashboard() {
#ifdef EPAPER_ENABLE
    Serial1.println("Drawing dashboard...");
    
    // Handle HA before long E-Paper operation | 在长时间墨水屏操作前处理 HA
    ha.handle();
    
    // Clear screen | 清屏
    epaper.fillScreen(TFT_WHITE);
    
    // Draw header | 绘制标题栏
    drawHeader();
    
    // Calculate card layout | 计算卡片布局
    // 3 columns x 2 rows = 6 cards | 3列 x 2行 = 6张卡片
    int startX = (SCREEN_WIDTH - (3 * CARD_WIDTH + 2 * CARD_MARGIN)) / 2;
    int startY = HEADER_HEIGHT + 30;
    
    int cardIndex = 0;
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            int x = startX + col * (CARD_WIDTH + CARD_MARGIN);
            int y = startY + row * (CARD_HEIGHT + CARD_MARGIN);
            
            if (cardIndex < entityCount) {
                drawEntityCard(x, y, displayEntities[cardIndex]);
            } else {
                drawEmptyCard(x, y);
            }
            cardIndex++;
        }
    }
    
    // Draw footer | 绘制底部
    drawFooter();
    
    // Handle HA one more time before blocking update | 阻塞更新前再处理一次 HA
    ha.handle();
    
    // Update E-Paper display (this is BLOCKING and takes several seconds!)
    // 更新墨水屏（这是阻塞操作，需要几秒钟！）
    Serial1.println("Updating E-Paper display (this may take a while)...");
    epaper.update();
    Serial1.println("Display updated!");
    
    // Immediately handle HA after update to restore connection
    // 更新后立即处理 HA 以恢复连接
    for (int i = 0; i < 5; i++) {
        ha.handle();
        delay(10);
    }
#endif
}

/**
 * Draw startup screen (Page 1)
 * 绘制启动画面（页面1）
 * Shows WiFi config and waits for HA connection
 * 显示 WiFi 配置并等待 HA 连接
 */
void drawStartupScreen(const char* status, const char* ip = nullptr) {
#ifdef EPAPER_ENABLE
    epaper.fillScreen(TFT_WHITE);

    // Logo area | Logo 区域
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2 - 30;
    
    // Decorative circles | 装饰圆圈
    epaper.drawCircle(centerX, centerY - 70, 60, TFT_BLUE);
    epaper.drawCircle(centerX, centerY - 70, 55, TFT_BLUE);
    epaper.fillCircle(centerX, centerY - 70, 45, TFT_GREEN);
    
    // HA text inside circle | 圆内 HA 文字
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(4);
    epaper.drawString("HA", centerX - 25, centerY - 85);
    
    // Title | 标题
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(3);
    epaper.drawString("Home Assistant", centerX - 130, centerY + 10);
    epaper.setTextSize(2);
    epaper.drawString("Seeed HA Discovery", centerX - 100, centerY + 50);
    
    // Status text | 状态文字
        epaper.setTextColor(TFT_BLUE);
    epaper.setTextSize(2);
    epaper.drawString(status, centerX - 150, centerY + 100);
    
    // IP address if available | 如果有 IP 地址则显示
    if (ip != nullptr) {
        epaper.setTextColor(TFT_BLACK);
        epaper.setTextSize(2);
        epaper.drawString("IP: ", centerX - 100, centerY + 140);
        epaper.drawString(ip, centerX - 60, centerY + 140);
    }
    
    // WiFi config info | WiFi 配置信息
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(1);
    epaper.drawString("WiFi SSID: " + String(WIFI_SSID), 20, SCREEN_HEIGHT - 60);
    
    // Colored bars at bottom | 底部彩色条
    int barWidth = SCREEN_WIDTH / 6;
    epaper.fillRect(0 * barWidth, SCREEN_HEIGHT - 20, barWidth, 20, TFT_RED);
    epaper.fillRect(1 * barWidth, SCREEN_HEIGHT - 20, barWidth, 20, TFT_GREEN);
    epaper.fillRect(2 * barWidth, SCREEN_HEIGHT - 20, barWidth, 20, TFT_BLUE);
    epaper.fillRect(3 * barWidth, SCREEN_HEIGHT - 20, barWidth, 20, TFT_YELLOW);
    epaper.fillRect(4 * barWidth, SCREEN_HEIGHT - 20, barWidth, 20, TFT_BLACK);
    epaper.fillRect(5 * barWidth, SCREEN_HEIGHT - 20, barWidth, 20, TFT_WHITE);
    epaper.drawRect(5 * barWidth, SCREEN_HEIGHT - 20, barWidth, 20, TFT_BLACK);
    
    epaper.update();
#endif
}

/**
 * Draw WiFi provisioning screen (Colorful 6-color version)
 * 绘制 WiFi 配网界面（六色彩色版）
 */
void drawProvisioningScreen() {
#ifdef EPAPER_ENABLE
    epaper.fillScreen(TFT_WHITE);
    
    int centerX = SCREEN_WIDTH / 2;
    
    // Colorful header bar with gradient effect | 彩色标题栏带渐变效果
    epaper.fillRect(0, 0, SCREEN_WIDTH, 80, TFT_BLUE);
    epaper.fillRect(0, 70, SCREEN_WIDTH, 10, TFT_GREEN);
    
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("WiFi Setup Required", centerX - 180, 25);
    
    // WiFi icon with colors | 彩色 WiFi 图标
    int iconX = centerX;
    int iconY = 150;
    epaper.drawCircle(iconX, iconY + 30, 60, TFT_BLUE);
    epaper.drawCircle(iconX, iconY + 30, 58, TFT_BLUE);
    epaper.drawCircle(iconX, iconY + 30, 45, TFT_GREEN);
    epaper.drawCircle(iconX, iconY + 30, 43, TFT_GREEN);
    epaper.drawCircle(iconX, iconY + 30, 30, TFT_YELLOW);
    epaper.drawCircle(iconX, iconY + 30, 28, TFT_YELLOW);
    epaper.fillCircle(iconX, iconY + 30, 12, TFT_RED);
    // Mask bottom half | 遮住下半部分
    epaper.fillRect(iconX - 70, iconY + 30, 140, 70, TFT_WHITE);
    
    // Main instruction box with colored border | 带彩色边框的说明框
    int boxY = 220;
    int boxHeight = 180;
    epaper.fillRect(50, boxY, SCREEN_WIDTH - 100, boxHeight, TFT_WHITE);
    epaper.drawRect(50, boxY, SCREEN_WIDTH - 100, boxHeight, TFT_BLUE);
    epaper.drawRect(51, boxY + 1, SCREEN_WIDTH - 102, boxHeight - 2, TFT_BLUE);
    epaper.drawRect(52, boxY + 2, SCREEN_WIDTH - 104, boxHeight - 4, TFT_GREEN);
    
    // Step 1 with red number | 红色序号的步骤1
    epaper.fillCircle(100, boxY + 40, 18, TFT_RED);
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(2);
    epaper.drawString("1", 94, boxY + 32);
    
    epaper.setTextColor(TFT_BLACK);
    epaper.drawString("Connect to WiFi hotspot:", 130, boxY + 32);
    epaper.setTextSize(3);
    epaper.setTextColor(TFT_BLUE);
    epaper.drawString(AP_SSID, 130, boxY + 58);
    
    // Separator line | 分隔线
    epaper.drawLine(80, boxY + 95, SCREEN_WIDTH - 80, boxY + 95, TFT_BLACK);
    
    // Step 2 with red number | 红色序号的步骤2
    epaper.fillCircle(100, boxY + 130, 18, TFT_RED);
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(2);
    epaper.drawString("2", 94, boxY + 122);
    
    epaper.setTextColor(TFT_BLACK);
    epaper.drawString("Open browser and visit:", 130, boxY + 115);
    epaper.setTextSize(3);
    epaper.setTextColor(TFT_GREEN);
    epaper.drawString("http://192.168.4.1", 130, boxY + 140);
    
    // Bottom instruction | 底部说明
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(2);
    epaper.drawString("Select your WiFi network and enter password", centerX - 230, boxY + boxHeight + 20);
    
    // Colorful footer bar | 彩色底部条
    int barWidth = SCREEN_WIDTH / 6;
    epaper.fillRect(0 * barWidth, SCREEN_HEIGHT - 30, barWidth, 30, TFT_RED);
    epaper.fillRect(1 * barWidth, SCREEN_HEIGHT - 30, barWidth, 30, TFT_GREEN);
    epaper.fillRect(2 * barWidth, SCREEN_HEIGHT - 30, barWidth, 30, TFT_BLUE);
    epaper.fillRect(3 * barWidth, SCREEN_HEIGHT - 30, barWidth, 30, TFT_YELLOW);
    epaper.fillRect(4 * barWidth, SCREEN_HEIGHT - 30, barWidth, 30, TFT_BLACK);
    epaper.fillRect(5 * barWidth, SCREEN_HEIGHT - 30, barWidth, 30, TFT_WHITE);
    epaper.drawRect(5 * barWidth, SCREEN_HEIGHT - 30, barWidth, 30, TFT_BLACK);
    
    epaper.update();
#endif
}

/**
 * Draw WiFi connected screen (Colorful version)
 * 绘制 WiFi 已连接界面（彩色版）
 */
void drawWiFiConnectedScreen(const char* ip) {
#ifdef EPAPER_ENABLE
    epaper.fillScreen(TFT_WHITE);
    
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2 - 20;
    
    // Success header with green | 绿色成功标题
    epaper.fillRect(0, 0, SCREEN_WIDTH, 80, TFT_GREEN);
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("WiFi Connected!", centerX - 130, 25);
    
    // Large checkmark icon with colors | 彩色大对勾图标
    int checkX = centerX;
    int checkY = 170;
    epaper.fillCircle(checkX, checkY, 55, TFT_GREEN);
    epaper.fillCircle(checkX, checkY, 45, TFT_WHITE);
    epaper.fillCircle(checkX, checkY, 40, TFT_GREEN);
    // Draw checkmark | 绘制对勾
    for (int i = 0; i < 4; i++) {
        epaper.drawLine(checkX - 25 + i, checkY, checkX - 5 + i, checkY + 20, TFT_WHITE);
        epaper.drawLine(checkX - 5 + i, checkY + 20, checkX + 30 + i, checkY - 20, TFT_WHITE);
    }
    
    // IP address display with blue box | 蓝色框显示 IP 地址
    int ipBoxY = 260;
    epaper.fillRect(centerX - 180, ipBoxY, 360, 60, TFT_BLUE);
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(2);
    epaper.drawString("Device IP Address:", centerX - 100, ipBoxY + 8);
    epaper.setTextSize(3);
    epaper.drawString(ip, centerX - 100, ipBoxY + 30);
    
    // Instructions box | 说明框
    int boxY = 350;
    epaper.fillRect(100, boxY, SCREEN_WIDTH - 200, 70, TFT_YELLOW);
    epaper.drawRect(100, boxY, SCREEN_WIDTH - 200, 70, TFT_BLACK);
    
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(2);
    epaper.drawString("Add this device in Home Assistant:", centerX - 180, boxY + 12);
    epaper.drawString("Settings -> Devices & Services", centerX - 160, boxY + 40);
    
    // Footer | 底部
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(1);
    epaper.drawString("Waiting for Home Assistant connection...", centerX - 130, SCREEN_HEIGHT - 40);
    
    epaper.update();
#endif
}

// =============================================================================
// HA State Management | HA 状态管理
// =============================================================================

/**
 * Update display entities from HA states
 * 从 HA 状态更新显示实体
 */
void updateDisplayEntities() {
    const auto& states = ha.getHAStates();
    entityCount = 0;
    
    for (const auto& pair : states) {
        if (entityCount >= MAX_DISPLAY_ENTITIES) break;
        
        SeeedHAState* state = pair.second;
        EntityDisplay& entity = displayEntities[entityCount];
        
        entity.entityId = state->getEntityId();
        entity.friendlyName = state->getFriendlyName();
        entity.state = state->getString();
        entity.unit = state->getUnit();
        entity.hasValue = state->hasValue();
        entity.color = getColorForDeviceClass(state->getDeviceClass());
        
        entityCount++;
    }
    
    Serial1.printf("Updated %d entities for display\n", entityCount);
}

// =============================================================================
// Setup & Loop | 设置与主循环
// =============================================================================

void setup() {
    // reTerminal E1002 uses Serial1 for debug output with specific pins
    // reTerminal E1002 使用 Serial1 作为调试输出，需要指定引脚
    Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
    delay(1000);
    
    Serial1.println();
    Serial1.println("============================================");
    Serial1.println("  reTerminal E1002 - HA Display Dashboard");
    Serial1.println("  (6-Color E-Paper)");
    Serial1.println("============================================");
    
#ifdef EPAPER_ENABLE
    // Initialize E-Paper | 初始化墨水屏
    Serial1.println("Initializing E-Paper display...");
    epaper.begin();
    
    // Page 1: Show startup screen immediately with "Initializing..."
    // 页面1：立即显示启动画面，状态为"正在初始化..."
    Serial1.println("Displaying startup screen...");
    drawStartupScreen("Initializing...");
    
#else
    Serial1.println("WARNING: EPAPER_ENABLE not defined!");
    Serial1.println("Make sure to enable E-Paper in User_Setup.h");
#endif
    
    // Initialize GPIO pins | 初始化 GPIO 引脚
    Serial1.println("Initializing GPIO pins...");
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);  // LED off (active LOW)
    pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
    Serial1.println("  - Status LED (GPIO6): Initialized");
    Serial1.println("  - Reset Button (GPIO3): Initialized");
    Serial1.println("  - Buzzer (GPIO45): Ready");
    
    // Create RTOS task for reset button on Core 0 (main loop runs on Core 1)
    // 在 Core 0 创建重置按钮的 RTOS 任务（主循环在 Core 1）
    xTaskCreatePinnedToCore(
        resetButtonTask,        // Task function | 任务函数
        "ResetButton",          // Task name | 任务名称
        4096,                   // Stack size | 堆栈大小
        NULL,                   // Parameters | 参数
        1,                      // Priority | 优先级
        &resetButtonTaskHandle, // Task handle | 任务句柄
        0                       // Core 0 | 核心 0
    );
    Serial1.println("  - Reset button task started on Core 0");
    
    // Brief LED blink to indicate boot | 短暂 LED 闪烁指示启动
    for (int i = 0; i < 2; i++) {
        setStatusLED(true);
        delay(100);
        setStatusLED(false);
        delay(100);
    }
    
    // Enable debug | 启用调试
    ha.enableDebug(true);
    
    // Set device info | 设置设备信息
    ha.setDeviceInfo(
        "HA Display",       // Device name | 设备名称
        "reTerminal E1002", // Model | 型号
        "1.1.0"             // Version | 版本
    );
    
    // Register HA state callback | 注册 HA 状态回调
    ha.onHAState([](const char* entityId, const char* state, JsonObject& attrs) {
        Serial1.printf("HA State received: %s = %s\n", entityId, state);
        
        int currentCount = ha.getHAStates().size();
        
        // Initial data collection | 初始数据收集
        if (!initialRefreshDone) {
            if (!dataReceived) {
                dataReceived = true;
                dataReceivedTime = millis();
                Serial1.println("First data received, waiting for more entities...");
            }
            // Reset timer if new entity arrives | 如果有新实体到达，重置计时器
            if (currentCount > lastEntityCount) {
                lastEntityCount = currentCount;
                dataReceivedTime = millis();
                Serial1.printf("Entity count: %d, resetting wait timer...\n", currentCount);
            }
        }
        // Config change detection (after initial refresh)
        // 配置变更检测（初始刷新后）
        else if (configChanged) {
            if (currentCount > lastEntityCount) {
                lastEntityCount = currentCount;
                configChangeTime = millis();
                Serial1.printf("Config update: %d entities, resetting wait timer...\n", currentCount);
            }
        }
        // Normal state update (value change only) - no refresh needed
        // 普通状态更新（仅值变化）- 不需要刷新
        else {
            Serial1.println("Value update only, no refresh needed.");
        }
    });
    
    // Connect to WiFi | 连接 WiFi
    Serial1.println();
    
#if USE_WIFI_PROVISIONING
    // Use web-based WiFi provisioning | 使用网页配网
    Serial1.println("Starting with WiFi provisioning...");
    Serial1.print("  AP Name (if needed): ");
    Serial1.println(AP_SSID);
    
    bool wifiConnected = ha.beginWithProvisioning(AP_SSID);
    
    // Enable reset button | 启用重置按钮
    ha.enableResetButton(PIN_RESET_BUTTON);
    
    if (!wifiConnected) {
        // Device is in AP mode for WiFi configuration
        // 设备处于 AP 模式进行 WiFi 配置
        Serial1.println();
        Serial1.println("============================================");
        Serial1.println("  WiFi Provisioning Mode Active!");
        Serial1.println("============================================");
        Serial1.println();
        Serial1.println("To configure WiFi:");
        Serial1.println("  1. Connect to WiFi: " + String(AP_SSID));
        Serial1.println("  2. Open browser: http://192.168.4.1");
        Serial1.println("  3. Select network and enter password");
        Serial1.println();
        
        wifiProvisioningMode = true;
        
        // LED slow blink to indicate provisioning mode | LED 慢闪指示配网模式
        for (int i = 0; i < 3; i++) {
            setStatusLED(true);
            delay(300);
            setStatusLED(false);
            delay(300);
        }
        
#ifdef EPAPER_ENABLE
        Serial1.println("Displaying provisioning screen on E-Paper...");
        drawProvisioningScreen();
#endif
        return;
    }
#else
    // Use hardcoded credentials | 使用硬编码凭据
    Serial1.println("Connecting to WiFi...");
    Serial1.print("  SSID: ");
    Serial1.println(WIFI_SSID);
    
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial1.println("WiFi connection failed!");
#ifdef EPAPER_ENABLE
        drawStartupScreen("WiFi Connection Failed!");
#endif
        while (1) delay(1000);
    }
#endif
    
    Serial1.println("WiFi connected!");
    Serial1.print("IP: ");
    Serial1.println(ha.getLocalIP());
    
    // LED quick blinks to indicate WiFi connected | LED 快闪指示 WiFi 已连接
    for (int i = 0; i < 3; i++) {
        setStatusLED(true);
        delay(100);
        setStatusLED(false);
        delay(100);
    }
    
#ifdef EPAPER_ENABLE
    // Update startup screen with IP and waiting for HA status
    // 更新启动画面，显示 IP 和等待 HA 状态
    Serial1.println("Updating startup screen with IP...");
    drawWiFiConnectedScreen(ha.getLocalIP().toString().c_str());
#endif
    
    Serial1.println();
    Serial1.println("============================================");
    Serial1.println("  Setup Complete!");
    Serial1.println("============================================");
    Serial1.println();
    Serial1.println("Now configure entity subscriptions in HA:");
    Serial1.println("1. Go to Settings > Devices & Services");
    Serial1.println("2. Find this device and click Configure");
    Serial1.println("3. Select entities to subscribe");
    Serial1.println();
}

void loop() {
    // Handle HA communication | 处理 HA 通信
    ha.handle();
    
    // Check if WiFi reset was requested by the button task (on Core 0)
    // 检查按钮任务（Core 0）是否请求了 WiFi 重置
    if (wifiResetRequested) {
        wifiResetRequested = false;
        Serial1.println("  Clearing credentials and restarting...");
        ha.clearWiFiCredentials();
        Serial1.flush();
        delay(500);
        ESP.restart();
    }
    
    // Detect if we entered AP mode due to reset button press
    // 检测是否因按下重置按钮而进入了 AP 模式
    if (!wifiProvisioningMode && ha.isProvisioningActive()) {
        Serial1.println();
        Serial1.println("============================================");
        Serial1.println("  Entered AP Mode (WiFi Reset Triggered)!");
        Serial1.println("============================================");
        Serial1.println("  Connect to AP: " + String(AP_SSID));
        Serial1.println("  Then visit: http://192.168.4.1");
        Serial1.println();
        
        wifiProvisioningMode = true;
        
        // Reset display state | 重置显示状态
        initialRefreshDone = false;
        dataReceived = false;
        lastEntityCount = 0;
        
#ifdef EPAPER_ENABLE
        Serial1.println("Updating E-Paper to show provisioning screen...");
        drawProvisioningScreen();
#endif
    }
    
    // If in provisioning mode, handle it specially | 如果处于配网模式，特殊处理
    if (wifiProvisioningMode) {
        // Check if WiFi got connected (user completed provisioning)
        // 检查 WiFi 是否已连接（用户完成了配网）
        if (ha.isWiFiConnected()) {
            Serial1.println();
            Serial1.println("============================================");
            Serial1.println("  WiFi Connected via Provisioning!");
            Serial1.println("============================================");
            Serial1.print("IP: ");
            Serial1.println(ha.getLocalIP());
            
            wifiProvisioningMode = false;
            
            // LED quick blinks to indicate WiFi connected | LED 快闪指示 WiFi 已连接
            for (int i = 0; i < 5; i++) {
                setStatusLED(true);
                delay(100);
                setStatusLED(false);
                delay(100);
            }
            
#ifdef EPAPER_ENABLE
            Serial1.println("Updating E-Paper to show connected status...");
            drawWiFiConnectedScreen(ha.getLocalIP().toString().c_str());
#endif
            
            Serial1.println();
            Serial1.println("Now configure entity subscriptions in HA:");
            Serial1.println("1. Go to Settings > Devices & Services");
            Serial1.println("2. Find this device and click Configure");
            Serial1.println("3. Select entities to subscribe");
            Serial1.println();
        }
        
        // Print status periodically in provisioning mode | 配网模式下定期打印状态
        static unsigned long lastProvisioningStatus = 0;
        unsigned long now = millis();
        if (now - lastProvisioningStatus > 30000) {
            lastProvisioningStatus = now;
            Serial1.println("Status: WiFi Provisioning mode active...");
            Serial1.println("  Connect to AP: " + String(AP_SSID));
            Serial1.println("  Then visit: http://192.168.4.1");
        }
        
        return;
    }
    
    unsigned long now = millis();
    int currentEntityCount = ha.getHAStates().size();
    
    // Detect config clear (entity count drops to 0)
    // 检测配置清除（实体数量降为0）
    if (initialRefreshDone && !configChanged && lastEntityCount > 0 && currentEntityCount == 0) {
        Serial1.println("Config cleared! Waiting for new entities...");
        configChanged = true;
        configChangeTime = now;
        lastEntityCount = 0;
    }
    
    // E-Paper refresh logic | 墨水屏刷新逻辑
    bool shouldRefresh = false;
    
    // 0. HA connection status change - with debounce to prevent flickering
    // 0. HA 连接状态变化 - 带防抖防止频繁刷新
    bool currentHAConnected = ha.isHAConnected();
    if (initialRefreshDone && lastHAConnected != currentHAConnected) {
        // Status changed, start debounce timer | 状态变化，开始防抖计时
        if (!haStatusPendingRefresh) {
            haStatusPendingRefresh = true;
            haStatusChangeTime = now;
            if (currentHAConnected) {
                Serial1.println("HA connected! Waiting for stable connection...");
            } else {
                Serial1.println("HA disconnected! Waiting to confirm...");
            }
        }
    }
    
    // Check if debounce period passed and status is stable | 检查防抖时间是否过去且状态稳定
    if (haStatusPendingRefresh && (now - haStatusChangeTime >= HA_STATUS_DEBOUNCE)) {
        // Status has been stable for debounce period | 状态已稳定超过防抖时间
        if (currentHAConnected != lastHAConnected) {
            if (currentHAConnected) {
                Serial1.println("HA connection stable! Refreshing display...");
            } else {
                Serial1.println("HA disconnection confirmed! Refreshing display...");
            }
            shouldRefresh = true;
            lastDisplayUpdate = now;
            lastHAConnected = currentHAConnected;
        }
        haStatusPendingRefresh = false;
    } else if (haStatusPendingRefresh && currentHAConnected == lastHAConnected) {
        // Status reverted back, cancel pending refresh | 状态恢复，取消待刷新
        Serial1.println("HA status reverted, canceling refresh.");
        haStatusPendingRefresh = false;
    }
    
    // 1. Initial refresh: wait for data collection period after first data
    // 1. 初始刷新：收到第一批数据后等待收集期结束
    if (!shouldRefresh && !initialRefreshDone && dataReceived) {
        if (now - dataReceivedTime >= DATA_COLLECTION_WAIT) {
            Serial1.printf("Data collection complete! %d entities collected.\n", currentEntityCount);
            Serial1.println("Performing initial refresh...");
            shouldRefresh = true;
            initialRefreshDone = true;
            lastDisplayUpdate = now;
            lastEntityCount = currentEntityCount;
        }
    }
    // 2. Config change refresh: user added/removed entities in HA
    // 2. 配置变更刷新：用户在 HA 中添加/删除实体
    else if (!shouldRefresh && configChanged && currentEntityCount > 0) {
        // Wait for data collection after config change
        // 配置变更后等待数据收集
        if (now - configChangeTime >= DATA_COLLECTION_WAIT) {
            Serial1.printf("Config update complete! %d entities.\n", currentEntityCount);
            Serial1.println("Refreshing for config change...");
            shouldRefresh = true;
            configChanged = false;
            lastDisplayUpdate = now;
            lastEntityCount = currentEntityCount;
        }
    }
    // 3. Periodic refresh: every DISPLAY_REFRESH_INTERVAL (5 minutes)
    // 3. 定时刷新：每 DISPLAY_REFRESH_INTERVAL（5分钟）
    else if (!shouldRefresh && initialRefreshDone && !configChanged && (now - lastDisplayUpdate >= DISPLAY_REFRESH_INTERVAL)) {
        Serial1.println("Periodic refresh triggered...");
        shouldRefresh = true;
        lastDisplayUpdate = now;
    }
    
    if (shouldRefresh) {
        updateDisplayEntities();
        drawDashboard();
    }
    
    // Print status periodically | 定期打印状态
    static unsigned long lastStatusPrint = 0;
    if (now - lastStatusPrint > 30000) {
        lastStatusPrint = now;
        if (configChanged) {
            Serial1.printf("Status: Config updating... %d entities, refresh in: %lu sec\n",
                          currentEntityCount,
                          (DATA_COLLECTION_WAIT - (now - configChangeTime)) / 1000);
        } else if (initialRefreshDone) {
            Serial1.printf("Status: HA %s, Entities: %d, Next refresh in: %lu sec\n", 
                          ha.isHAConnected() ? "Connected" : "Waiting", 
                          entityCount,
                          (DISPLAY_REFRESH_INTERVAL - (now - lastDisplayUpdate)) / 1000);
        } else if (dataReceived) {
            Serial1.printf("Status: Collecting data... %d entities, refresh in: %lu sec\n",
                          currentEntityCount,
                          (DATA_COLLECTION_WAIT - (now - dataReceivedTime)) / 1000);
        } else {
            Serial1.println("Status: Waiting for HA data...");
        }
    }
}
