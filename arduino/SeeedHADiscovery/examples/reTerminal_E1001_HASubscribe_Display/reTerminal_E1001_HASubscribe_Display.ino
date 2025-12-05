/**
 * ============================================================================
 * reTerminal E1001 - HA State Subscribe with E-Paper Display (4-Level Grayscale)
 * reTerminal E1001 - HA 状态订阅墨水屏显示示例（四阶灰度）
 * ============================================================================
 *
 * This example demonstrates how to:
 * 本示例展示如何：
 * 1. Subscribe to Home Assistant entity states
 *    订阅 Home Assistant 实体状态
 * 2. Display the states on a 4-level grayscale E-Paper display (800x480)
 *    在四阶灰度墨水屏上显示状态（800x480）
 * 3. Create a clean monochrome dashboard UI
 *    创建简洁的单色仪表板界面
 *
 * Hardware:
 * 硬件：
 * - reTerminal E1001 with 4-level grayscale E-Paper display
 *   reTerminal E1001 带四阶灰度墨水屏
 * - Display resolution: 800x480
 *   显示分辨率：800x480
 *
 * Supported Colors (4 grayscale levels only):
 * 支持的颜色（仅4阶灰度）：
 * - TFT_GRAY_0  (黑色 / Black)
 * - TFT_GRAY_1  (深灰 / Dark Gray)
 * - TFT_GRAY_2  (浅灰 / Light Gray)
 * - TFT_GRAY_3  (白色 / White)
 *
 * Dependencies:
 * 依赖库：
 * - Seeed_GFX: https://github.com/Seeed-Studio/Seeed_GFX
 * - SeeedHADiscovery
 *
 * Setup Steps:
 * 设置步骤：
 * 1. Configure WiFi credentials below
 *    配置下方的 WiFi 凭据
 * 2. Upload to reTerminal E1001
 *    上传到 reTerminal E1001
 * 3. In Home Assistant, find your device and click "Configure"
 *    在 Home Assistant 中找到设备并点击"配置"
 * 4. Subscribe to entities (e.g., temperature, humidity sensors)
 *    订阅实体（如温度、湿度传感器）
 * 5. The display will update automatically
 *    显示屏将自动更新
 *
 * @author Seeed Studio
 * @version 1.0.0
 */

#include <SeeedHADiscovery.h>
#include "TFT_eSPI.h"

// =============================================================================
// Configuration | 配置
// =============================================================================

// WiFi credentials | WiFi 凭据
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";

// Display refresh interval (ms) - E-Paper refresh is slow, don't update too often
// 显示刷新间隔（毫秒）- 墨水屏刷新较慢，不要太频繁
const unsigned long DISPLAY_REFRESH_INTERVAL = 300000;  // 5 minutes | 5分钟

// Serial1 pin configuration for reTerminal E1001
// reTerminal E1001 的 Serial1 引脚配置
#define SERIAL_RX 44
#define SERIAL_TX 43

// =============================================================================
// Display Configuration | 显示配置
// =============================================================================

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

// Grayscale color aliases for better readability
// 灰度颜色别名，提高可读性
#define COLOR_BLACK     TFT_GRAY_0  // 黑色
#define COLOR_DARK      TFT_GRAY_1  // 深灰
#define COLOR_LIGHT     TFT_GRAY_2  // 浅灰
#define COLOR_WHITE     TFT_GRAY_3  // 白色

// Screen dimensions | 屏幕尺寸
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 480;

// Layout constants | 布局常量
const int HEADER_HEIGHT = 70;
const int CARD_WIDTH = 240;
const int CARD_HEIGHT = 150;
const int CARD_MARGIN = 20;
const int CARD_PADDING = 15;

// =============================================================================
// Global Variables | 全局变量
// =============================================================================

SeeedHADiscovery ha;
unsigned long lastDisplayUpdate = 0;
bool initialRefreshDone = false;

// Wait time after receiving data before refresh (to collect all entities)
// 收到数据后等待一段时间再刷新（以便收集所有实体）
const unsigned long DATA_COLLECTION_WAIT = 5000;  // 5 seconds | 5秒
unsigned long dataReceivedTime = 0;
bool dataReceived = false;
int lastEntityCount = 0;

// Config change detection | 配置变更检测
bool configChanged = false;
unsigned long configChangeTime = 0;

// HA connection status tracking | HA 连接状态跟踪
bool lastHAConnected = false;

// Entity data structure | 实体数据结构
struct EntityDisplay {
    String entityId;
    String friendlyName;
    String state;
    String unit;
    String deviceClass;
    bool hasValue;
};

// Maximum entities to display | 最大显示实体数
const int MAX_DISPLAY_ENTITIES = 6;
EntityDisplay displayEntities[MAX_DISPLAY_ENTITIES];
int entityCount = 0;

// =============================================================================
// UI Drawing Functions | UI 绘制函数
// =============================================================================

/**
 * Get icon character based on device class (simple text icons)
 * 根据设备类别获取图标字符（简单文字图标）
 */
const char* getIconForDeviceClass(const String& deviceClass) {
    if (deviceClass == "temperature") {
        return "TEMP";
    } else if (deviceClass == "humidity") {
        return "HUM";
    } else if (deviceClass == "battery") {
        return "BAT";
    } else if (deviceClass == "illuminance") {
        return "LUX";
    } else if (deviceClass == "power") {
        return "PWR";
    } else if (deviceClass == "energy") {
        return "NRG";
    } else if (deviceClass == "pressure") {
        return "HPA";
    } else if (deviceClass == "voltage") {
        return "V";
    } else if (deviceClass == "current") {
        return "A";
    } else {
        return "VAL";
    }
}

/**
 * Draw header bar with gradient effect
 * 绘制带渐变效果的顶部标题栏
 */
void drawHeader() {
#ifdef EPAPER_ENABLE
    // Header background with gradient effect | 带渐变效果的标题栏背景
    epaper.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT - 10, COLOR_BLACK);
    epaper.fillRect(0, HEADER_HEIGHT - 10, SCREEN_WIDTH, 5, COLOR_DARK);
    epaper.fillRect(0, HEADER_HEIGHT - 5, SCREEN_WIDTH, 5, COLOR_LIGHT);
    
    // Title text | 标题文字
    epaper.setTextColor(COLOR_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("Home Assistant Dashboard", 30, 20);
    
    // Connection status | 连接状态
    epaper.setTextSize(2);
    String status = ha.isHAConnected() ? "Online" : "Offline";
    epaper.drawString(status, SCREEN_WIDTH - 120, 25);
    
    // Status indicator (filled or hollow circle) | 状态指示（实心或空心圆）
    if (ha.isHAConnected()) {
        epaper.fillCircle(SCREEN_WIDTH - 140, 32, 8, COLOR_WHITE);
    } else {
        epaper.drawCircle(SCREEN_WIDTH - 140, 32, 8, COLOR_WHITE);
        epaper.drawCircle(SCREEN_WIDTH - 140, 32, 6, COLOR_WHITE);
    }
#endif
}

/**
 * Draw a single entity card with grayscale design
 * 绘制单个实体卡片（灰度设计）
 */
void drawEntityCard(int x, int y, EntityDisplay& entity) {
#ifdef EPAPER_ENABLE
    // Card background | 卡片背景
    epaper.fillRect(x, y, CARD_WIDTH, CARD_HEIGHT, COLOR_WHITE);
    
    // Card border (double line for emphasis) | 卡片边框（双线强调）
    epaper.drawRect(x, y, CARD_WIDTH, CARD_HEIGHT, COLOR_BLACK);
    epaper.drawRect(x + 2, y + 2, CARD_WIDTH - 4, CARD_HEIGHT - 4, COLOR_DARK);
    
    // Top accent bar | 顶部强调条
    epaper.fillRect(x + 2, y + 2, CARD_WIDTH - 4, 6, COLOR_BLACK);
    
    // Icon badge | 图标徽章
    const char* icon = getIconForDeviceClass(entity.deviceClass);
    epaper.fillRect(x + CARD_PADDING, y + 20, 50, 25, COLOR_DARK);
    epaper.setTextColor(COLOR_WHITE);
    epaper.setTextSize(1);
    epaper.drawString(icon, x + CARD_PADDING + 5, y + 26);
    
    // Entity name | 实体名称
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(2);
    
    String displayName = entity.friendlyName;
    if (displayName.length() == 0) {
        displayName = entity.entityId;
    }
    // Truncate long names | 截断过长的名称
    if (displayName.length() > 16) {
        displayName = displayName.substring(0, 13) + "...";
    }
    epaper.drawString(displayName, x + CARD_PADDING + 60, y + 25);
    
    // Separator line | 分隔线
    epaper.drawLine(x + CARD_PADDING, y + 55, x + CARD_WIDTH - CARD_PADDING, y + 55, COLOR_LIGHT);
    
    // Value | 数值
    if (entity.hasValue) {
        epaper.setTextColor(COLOR_BLACK);
        epaper.setTextSize(5);
        
        String valueStr = entity.state;
        // Truncate decimals if too long | 如果太长则截断小数
        if (valueStr.length() > 6) {
            float val = valueStr.toFloat();
            valueStr = String(val, 1);
        }
        
        epaper.drawString(valueStr, x + CARD_PADDING, y + 70);
        
        // Unit with background | 带背景的单位
        if (entity.unit.length() > 0) {
            int valueWidth = valueStr.length() * 30;
            epaper.setTextSize(2);
            epaper.setTextColor(COLOR_DARK);
            epaper.drawString(entity.unit, x + CARD_PADDING + valueWidth + 5, y + 90);
        }
    } else {
        // Waiting for data | 等待数据
        epaper.setTextColor(COLOR_DARK);
        epaper.setTextSize(2);
        epaper.drawString("Waiting...", x + CARD_PADDING, y + 80);
    }
    
    // Status indicator at bottom right | 右下角状态指示
    int indicatorX = x + CARD_WIDTH - 20;
    int indicatorY = y + CARD_HEIGHT - 20;
    if (entity.hasValue) {
        epaper.fillCircle(indicatorX, indicatorY, 6, COLOR_BLACK);
    } else {
        epaper.drawCircle(indicatorX, indicatorY, 6, COLOR_DARK);
    }
#endif
}

/**
 * Draw empty card placeholder
 * 绘制空卡片占位符
 */
void drawEmptyCard(int x, int y) {
#ifdef EPAPER_ENABLE
    // Dashed border effect using dots | 使用点阵的虚线边框效果
    for (int i = x; i < x + CARD_WIDTH; i += 8) {
        epaper.drawPixel(i, y, COLOR_DARK);
        epaper.drawPixel(i, y + CARD_HEIGHT - 1, COLOR_DARK);
    }
    for (int i = y; i < y + CARD_HEIGHT; i += 8) {
        epaper.drawPixel(x, i, COLOR_DARK);
        epaper.drawPixel(x + CARD_WIDTH - 1, i, COLOR_DARK);
    }
    
    // Plus sign | 加号
    int centerX = x + CARD_WIDTH / 2;
    int centerY = y + CARD_HEIGHT / 2;
    
    // Horizontal line of plus | 加号横线
    epaper.fillRect(centerX - 20, centerY - 3, 40, 6, COLOR_LIGHT);
    // Vertical line of plus | 加号竖线
    epaper.fillRect(centerX - 3, centerY - 20, 6, 40, COLOR_LIGHT);
    
    // Hint text | 提示文字
    epaper.setTextColor(COLOR_DARK);
    epaper.setTextSize(1);
    epaper.drawString("Subscribe in HA", x + 70, y + CARD_HEIGHT - 30);
#endif
}

/**
 * Draw footer with device info
 * 绘制底部设备信息
 */
void drawFooter() {
#ifdef EPAPER_ENABLE
    int footerY = SCREEN_HEIGHT - 40;
    
    // Footer background | 底部背景
    epaper.fillRect(0, footerY, SCREEN_WIDTH, 40, COLOR_LIGHT);
    epaper.drawLine(0, footerY, SCREEN_WIDTH, footerY, COLOR_DARK);
    
    // Device info | 设备信息
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(1);
    
    String deviceInfo = "Device: " + ha.getDeviceId();
    epaper.drawString(deviceInfo, 20, footerY + 12);
    
    String ipInfo = "IP: " + ha.getLocalIP().toString();
    epaper.drawString(ipInfo, 280, footerY + 12);
    
    // Update info | 更新信息
    unsigned long uptime = millis() / 1000;
    String uptimeStr = "Uptime: " + String(uptime / 60) + "m";
    epaper.drawString(uptimeStr, 500, footerY + 12);
    
    // Entity count | 实体数量
    String entityInfo = "Entities: " + String(entityCount);
    epaper.drawString(entityInfo, SCREEN_WIDTH - 120, footerY + 12);
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
    
    // Clear screen (must do this before drawing in gray mode)
    // 清屏（在灰度模式下绘制前必须执行）
    epaper.fillScreen(COLOR_WHITE);
    
    // Draw header | 绘制标题栏
    drawHeader();
    
    // Calculate card layout | 计算卡片布局
    // 3 columns x 2 rows = 6 cards | 3列 x 2行 = 6张卡片
    int startX = (SCREEN_WIDTH - (3 * CARD_WIDTH + 2 * CARD_MARGIN)) / 2;
    int startY = HEADER_HEIGHT + 20;
    
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
 */
void drawStartupScreen(const char* status, const char* ip = nullptr) {
#ifdef EPAPER_ENABLE
    epaper.fillScreen(COLOR_WHITE);
    
    // Logo area | Logo 区域
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2 - 40;
    
    // Decorative circles (grayscale gradient) | 装饰圆圈（灰度渐变）
    epaper.fillCircle(centerX, centerY - 60, 70, COLOR_LIGHT);
    epaper.fillCircle(centerX, centerY - 60, 60, COLOR_DARK);
    epaper.fillCircle(centerX, centerY - 60, 50, COLOR_BLACK);
    
    // HA text inside circle | 圆内 HA 文字
    epaper.setTextColor(COLOR_WHITE);
    epaper.setTextSize(4);
    epaper.drawString("HA", centerX - 25, centerY - 75);
    
    // Title | 标题
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(3);
    epaper.drawString("Home Assistant", centerX - 130, centerY + 30);
    
    epaper.setTextColor(COLOR_DARK);
    epaper.setTextSize(2);
    epaper.drawString("Seeed HA Discovery", centerX - 100, centerY + 70);
    
    // Status box | 状态框
    int statusBoxY = centerY + 110;
    epaper.fillRect(centerX - 180, statusBoxY, 360, 40, COLOR_LIGHT);
    epaper.drawRect(centerX - 180, statusBoxY, 360, 40, COLOR_DARK);
    
    // Status text | 状态文字
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(2);
    epaper.drawString(status, centerX - 150, statusBoxY + 10);
    
    // IP address if available | 如果有 IP 地址则显示
    if (ip != nullptr) {
        epaper.setTextColor(COLOR_DARK);
        epaper.setTextSize(2);
        epaper.drawString("IP: ", centerX - 80, centerY + 170);
        epaper.setTextColor(COLOR_BLACK);
        epaper.drawString(ip, centerX - 40, centerY + 170);
    }
    
    // WiFi config info | WiFi 配置信息
    epaper.setTextColor(COLOR_DARK);
    epaper.setTextSize(1);
    epaper.drawString("WiFi SSID: " + String(WIFI_SSID), 30, SCREEN_HEIGHT - 60);
    
    // Grayscale bars at bottom (showing available colors) | 底部灰度条（展示可用颜色）
    int barWidth = SCREEN_WIDTH / 4;
    epaper.fillRect(0 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_BLACK);
    epaper.fillRect(1 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_DARK);
    epaper.fillRect(2 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_LIGHT);
    epaper.fillRect(3 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_WHITE);
    epaper.drawRect(3 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_BLACK);
    
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
        entity.deviceClass = state->getDeviceClass();
        entity.hasValue = state->hasValue();
        
        entityCount++;
    }
    
    Serial1.printf("Updated %d entities for display\n", entityCount);
}

// =============================================================================
// Setup & Loop | 设置与主循环
// =============================================================================

void setup() {
    // reTerminal E1001 uses Serial1 for debug output with specific pins
    // reTerminal E1001 使用 Serial1 作为调试输出，需要指定引脚
    Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
    delay(1000);
    
    Serial1.println();
    Serial1.println("============================================");
    Serial1.println("  reTerminal E1001 - HA Display Dashboard");
    Serial1.println("  (4-Level Grayscale E-Paper)");
    Serial1.println("============================================");
    
#ifdef EPAPER_ENABLE
    // Initialize E-Paper | 初始化墨水屏
    Serial1.println("Initializing E-Paper display...");
    epaper.begin();
    
    // Clear screen first | 先清屏
    epaper.fillScreen(TFT_WHITE);
    epaper.update();
    
    // Initialize grayscale mode (4 levels) | 初始化灰度模式（4阶）
    Serial1.println("Initializing grayscale mode...");
    epaper.initGrayMode(GRAY_LEVEL4);
    
    // Page 1: Show startup screen immediately with "Connecting to WiFi..."
    // 页面1：立即显示启动画面，状态为"正在连接 WiFi..."
    Serial1.println("Displaying startup screen...");
    drawStartupScreen("Connecting to WiFi...");
    
#else
    Serial1.println("WARNING: EPAPER_ENABLE not defined!");
    Serial1.println("Make sure to enable E-Paper in User_Setup.h");
#endif
    
    // Enable debug | 启用调试
    ha.enableDebug(true);
    
    // Set device info | 设置设备信息
    ha.setDeviceInfo(
        "HA Display",       // Device name | 设备名称
        "reTerminal E1001", // Model | 型号
        "1.0.0"             // Version | 版本
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
    Serial1.println("Connecting to WiFi...");
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial1.println("WiFi connection failed!");
#ifdef EPAPER_ENABLE
        drawStartupScreen("WiFi Connection Failed!");
#endif
        while (1) delay(1000);
    }
    
    Serial1.println("WiFi connected!");
    Serial1.print("IP: ");
    Serial1.println(ha.getLocalIP());
    
#ifdef EPAPER_ENABLE
    // Update startup screen with IP and waiting for HA status
    // 更新启动画面，显示 IP 和等待 HA 状态
    Serial1.println("Updating startup screen with IP...");
    drawStartupScreen("Waiting for Home Assistant...", ha.getLocalIP().toString().c_str());
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
    
    // 0. HA connection status change - refresh when HA connects or disconnects
    // 0. HA 连接状态变化 - 当 HA 上线或掉线时刷新
    bool currentHAConnected = ha.isHAConnected();
    if (initialRefreshDone && lastHAConnected != currentHAConnected) {
        if (currentHAConnected) {
            Serial1.println("HA connected! Refreshing display...");
        } else {
            Serial1.println("HA disconnected! Refreshing display...");
        }
        shouldRefresh = true;
        lastDisplayUpdate = now;
    }
    lastHAConnected = currentHAConnected;
    
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
