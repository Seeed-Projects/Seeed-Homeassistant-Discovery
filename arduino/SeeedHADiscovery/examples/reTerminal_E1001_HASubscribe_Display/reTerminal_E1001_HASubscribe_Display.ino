/**
 * ============================================================================
 * reTerminal E1001 - HA State Subscribe with E-Paper Display (4-Level Grayscale)
 * reTerminal E1001 - HA 状态订阅墨水屏显示示例（四阶灰度）
 * ============================================================================
 *
 * CODE STRUCTURE | 代码结构:
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │ SECTION 1: USER CONFIGURATION                                          │
 * │ 第1部分：用户配置                                                        │
 * │ - Device name, AP name, refresh interval, etc.                         │
 * │ - 设备名称、AP名称、刷新间隔等                                            │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ SECTION 2: USER CUSTOM UI (★ YOUR CODE HERE ★)                         │
 * │ 第2部分：用户自定义UI（★ 在这里写你的代码 ★）                              │
 * │ - Design your own dashboard layout                                     │
 * │ - 设计你自己的仪表板布局                                                  │
 * │ - Draw cards, icons, values as you like                                │
 * │ - 按你喜欢的方式绘制卡片、图标、数值                                       │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ SECTION 3: FRAMEWORK CODE (DO NOT MODIFY)                              │
 * │ 第3部分：框架代码（请勿修改）                                             │
 * │ - WiFi provisioning, HA communication, reset button, etc.              │
 * │ - WiFi配网、HA通信、重置按钮等                                            │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * HOW TO USE | 使用方法:
 * 1. Modify SECTION 1 to configure your device
 *    修改第1部分来配置你的设备
 * 2. Implement your UI in SECTION 2 (especially drawCustomDashboard())
 *    在第2部分实现你的UI（特别是 drawCustomDashboard() 函数）
 * 3. DO NOT touch SECTION 3 unless you know what you're doing
 *    除非你知道自己在做什么，否则不要修改第3部分
 *
 * AVAILABLE APIs FOR YOUR UI | 你的UI可用的API:
 * 
 * - Entity Access | 实体访问:
 *   int count = getEntityCount();                              // Get total count | 获取实体总数
 *   EntityData* entity = getEntity(0);                         // Get by index | 按索引获取
 *   EntityData* temp = getEntityById("sensor.temperature");    // Get by ID | 按ID获取 ★推荐★
 *   
 * - Entity Properties | 实体属性:
 *   entity->entityId        // e.g., "sensor.temperature"
 *   entity->friendlyName    // e.g., "Living Room Temperature"  
 *   entity->state           // e.g., "23.5"
 *   entity->unit            // e.g., "°C"
 *   entity->deviceClass     // e.g., "temperature", "humidity", "battery"
 *   entity->hasValue        // true if data received | 如果收到数据则为true
 *
 * - Status APIs | 状态API:
 *   isHAOnline()            // true if connected to HA | 如果连接到HA则为true
 *   getDeviceIP()           // Device IP address | 设备IP地址
 *   getDeviceId()           // Device ID | 设备ID
 *
 * - Display Colors (4-level grayscale) | 显示颜色（4阶灰度）:
 *   COLOR_BLACK, COLOR_DARK, COLOR_LIGHT, COLOR_WHITE
 *
 * HOW ENTITY SUBSCRIPTION WORKS | 实体订阅如何工作:
 * 
 * 1. In Home Assistant, go to: Settings > Devices & Services
 *    在 Home Assistant 中，进入：设置 > 设备与服务
 * 
 * 2. Find your device and click "Configure"
 *    找到你的设备并点击"配置"
 * 
 * 3. Select which entities to subscribe (e.g., sensor.living_room_temperature)
 *    选择要订阅的实体（如 sensor.living_room_temperature）
 * 
 * 4. Use getEntityById("sensor.living_room_temperature") in your code
 *    在代码中使用 getEntityById("sensor.living_room_temperature") 获取数据
 *
 * Hardware:
 * 硬件：
 * - reTerminal E1001 with 4-level grayscale E-Paper display (800x480)
 *   reTerminal E1001 带四阶灰度墨水屏（800x480）
 * - GPIO3: Reset button (long press 6s to reset WiFi)
 *   GPIO3：重置按钮（长按6秒重置WiFi）
 * - GPIO6: Status LED (active LOW)
 *   GPIO6：状态 LED（低电平点亮）
 * - GPIO45: Buzzer
 *   GPIO45：蜂鸣器
 *
 * @author Seeed Studio
 * @version 2.0.0
 */

#include <SeeedHADiscovery.h>
#include "TFT_eSPI.h"

// *****************************************************************************
// *                                                                           *
// *                    ██╗   ██╗███████╗███████╗██████╗                        *
// *                    ██║   ██║██╔════╝██╔════╝██╔══██╗                       *
// *                    ██║   ██║███████╗█████╗  ██████╔╝                       *
// *                    ██║   ██║╚════██║██╔══╝  ██╔══██╗                       *
// *                    ╚██████╔╝███████║███████╗██║  ██║                       *
// *                     ╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝                       *
// *                                                                           *
// *              SECTION 1: USER CONFIGURATION | 第1部分：用户配置               *
// *                                                                           *
// *****************************************************************************

// -----------------------------------------------------------------------------
// Device Configuration | 设备配置
// -----------------------------------------------------------------------------

// Device name shown in Home Assistant | 在 Home Assistant 中显示的设备名称
#define DEVICE_NAME "reTerminal_E1001"

// Device model | 设备型号
#define DEVICE_MODEL "reTerminal E1001"

// Firmware version | 固件版本
#define FIRMWARE_VERSION "2.0.0"

// -----------------------------------------------------------------------------
// WiFi Provisioning Configuration | WiFi 配网配置
// -----------------------------------------------------------------------------

// Set to true to enable web-based WiFi provisioning (recommended)
// Set to false to use hardcoded credentials below
// 设置为 true 启用网页配网（推荐）
// 设置为 false 使用下面的硬编码凭据
#define USE_WIFI_PROVISIONING true

// AP hotspot name for WiFi provisioning | 配网时的 AP 热点名称
const char* AP_SSID = "reTerminal_E1001_AP";

// Fallback WiFi credentials (only used if USE_WIFI_PROVISIONING is false)
// 备用 WiFi 凭据（仅在 USE_WIFI_PROVISIONING 为 false 时使用）
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";

// -----------------------------------------------------------------------------
// Display Configuration | 显示配置
// -----------------------------------------------------------------------------

// Display refresh interval (ms) - E-Paper refresh is slow, don't update too often
// 显示刷新间隔（毫秒）- 墨水屏刷新较慢，不要太频繁
const unsigned long DISPLAY_REFRESH_INTERVAL = 300000;  // 5 minutes | 5分钟

// Maximum entities to display | 最大显示实体数
const int MAX_ENTITIES = 6;

// Screen dimensions | 屏幕尺寸
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 480;


// *****************************************************************************
// *  PREDEFINITIONS (Required for User Code) | 预定义（用户代码所需）            *
// *****************************************************************************

// Display Colors (4-level grayscale) | 显示颜色（4阶灰度）
#define COLOR_BLACK     TFT_GRAY_0  // 黑色
#define COLOR_DARK      TFT_GRAY_1  // 深灰
#define COLOR_LIGHT     TFT_GRAY_2  // 浅灰
#define COLOR_WHITE     TFT_GRAY_3  // 白色

// E-Paper Display Object | 墨水屏显示对象
#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

// Entity Data Structure | 实体数据结构
struct EntityData {
    String entityId;      // e.g., "sensor.living_room_temperature"
    String friendlyName;  // e.g., "Living Room Temp"
    String state;         // e.g., "23.5"
    String unit;          // e.g., "°C"
    String deviceClass;   // e.g., "temperature", "humidity", "battery"
    bool hasValue;        // true if data has been received
};

// Framework variables (internal use) | 框架变量（内部使用）
SeeedHADiscovery ha;
EntityData _entities[MAX_ENTITIES];
int _entityCount = 0;

// API Function Declarations | API 函数声明
EntityData* getEntity(int index);
EntityData* getEntityById(const char* entityId);
EntityData* getEntityById(const String& entityId);
int getEntityCount();
bool isHAOnline();
String getDeviceIP();
String getDeviceId();


// *****************************************************************************
// *                                                                           *
// *               ██████╗██╗   ██╗███████╗████████╗ ██████╗ ███╗   ███╗       *
// *              ██╔════╝██║   ██║██╔════╝╚══██╔══╝██╔═══██╗████╗ ████║       *
// *              ██║     ██║   ██║███████╗   ██║   ██║   ██║██╔████╔██║       *
// *              ██║     ██║   ██║╚════██║   ██║   ██║   ██║██║╚██╔╝██║       *
// *              ╚██████╗╚██████╔╝███████║   ██║   ╚██████╔╝██║ ╚═╝ ██║       *
// *               ╚═════╝ ╚═════╝ ╚══════╝   ╚═╝    ╚═════╝ ╚═╝     ╚═╝       *
// *                                                                           *
// *       SECTION 2: USER CUSTOM UI | 第2部分：用户自定义UI                      *
// *                                                                           *
// *       ★★★ THIS IS WHERE YOU WRITE YOUR CODE ★★★                          *
// *       ★★★ 在这里编写你的代码 ★★★                                           *
// *                                                                           *
// *****************************************************************************

// =============================================================================
// API Usage Examples | API 使用示例
// =============================================================================
//
// ★ RECOMMENDED: Get entity by its ID | 推荐：通过实体ID获取 ★
// EntityData* temp = getEntityById("sensor.living_room_temperature");
// if (temp != NULL && temp->hasValue) {
//     Serial.println(temp->state);  // "23.5"
// }
//
// Get entity by index (for looping) | 按索引获取（用于循环）
// for (int i = 0; i < getEntityCount(); i++) {
//     EntityData* e = getEntity(i);
//     Serial.println(e->entityId);  // Print what entity this is
// }
//
// Other APIs | 其他API
// int getEntityCount();    // Number of subscribed entities | 订阅的实体数量
// bool isHAOnline();       // HA connection status | HA连接状态
// String getDeviceIP();    // Device IP address | 设备IP地址
// String getDeviceId();    // Device unique ID | 设备唯一ID

// =============================================================================
// YOUR CUSTOM DRAWING FUNCTIONS | 你的自定义绘图函数
// =============================================================================

// =============================================================================
// EXAMPLE: Fixed Layout Dashboard | 示例：固定布局仪表板
// =============================================================================
// If you want specific entities in specific positions, use getEntityById():
// 如果你想让特定实体显示在特定位置，使用 getEntityById()：
//
// void drawCustomDashboard() {
//     // Always show temperature in top-left card
//     // 温度始终显示在左上角卡片
//     EntityData* temp = getEntityById("sensor.living_room_temperature");
//     drawEntityCard(50, 100, 240, 150, temp);
//     
//     // Always show humidity in top-middle card
//     // 湿度始终显示在中间上方卡片
//     EntityData* hum = getEntityById("sensor.living_room_humidity");
//     drawEntityCard(310, 100, 240, 150, hum);
//     
//     // Always show battery in top-right card  
//     // 电池始终显示在右上角卡片
//     EntityData* bat = getEntityById("sensor.phone_battery");
//     drawEntityCard(570, 100, 240, 150, bat);
// }
//
// TIP: You can find entity IDs in Home Assistant:
// 提示：你可以在 Home Assistant 中找到实体ID：
// Settings > Devices & Services > Entities tab > copy the entity_id
// 设置 > 设备与服务 > 实体标签 > 复制 entity_id
// =============================================================================

/**
 * Draw a single entity card
 * 绘制单个实体卡片
 * 
 * Example implementation - modify as you like!
 * 示例实现 - 随意修改！
 * 
 * @param x      Card X position | 卡片X坐标
 * @param y      Card Y position | 卡片Y坐标
 * @param width  Card width | 卡片宽度
 * @param height Card height | 卡片高度
 * @param entity Entity data pointer (can be NULL if no entity) | 实体数据指针（如果没有实体可以为NULL）
 */
void drawEntityCard(int x, int y, int width, int height, EntityData* entity) {
#ifdef EPAPER_ENABLE
    // Card background | 卡片背景
    epaper.fillRect(x, y, width, height, COLOR_WHITE);
    
    // Card border | 卡片边框
    epaper.drawRect(x, y, width, height, COLOR_BLACK);
    epaper.drawRect(x + 2, y + 2, width - 4, height - 4, COLOR_DARK);
    
    // Top accent bar | 顶部强调条
    epaper.fillRect(x + 2, y + 2, width - 4, 6, COLOR_BLACK);
    
    if (entity != NULL && entity->hasValue) {
        // ----- Entity Name | 实体名称 -----
        epaper.setTextColor(COLOR_BLACK);
        epaper.setTextSize(2);
        
        String displayName = entity->friendlyName;
        if (displayName.length() == 0) {
            displayName = entity->entityId;
        }
        // Truncate long names | 截断过长的名称
        if (displayName.length() > 16) {
            displayName = displayName.substring(0, 13) + "...";
        }
        epaper.drawString(displayName, x + 15, y + 20);
        
        // ----- Device Class Badge | 设备类型徽章 -----
        const char* icon = "VAL";
        if (entity->deviceClass == "temperature") icon = "TEMP";
        else if (entity->deviceClass == "humidity") icon = "HUM";
        else if (entity->deviceClass == "battery") icon = "BAT";
        else if (entity->deviceClass == "illuminance") icon = "LUX";
        else if (entity->deviceClass == "power") icon = "PWR";
        else if (entity->deviceClass == "pressure") icon = "HPA";
        
        epaper.fillRect(x + 15, y + 50, 50, 25, COLOR_DARK);
        epaper.setTextColor(COLOR_WHITE);
        epaper.setTextSize(1);
        epaper.drawString(icon, x + 20, y + 56);
        
        // ----- Value | 数值 -----
        epaper.setTextColor(COLOR_BLACK);
        epaper.setTextSize(5);
        
        String valueStr = entity->state;
        if (valueStr.length() > 6) {
            float val = valueStr.toFloat();
            valueStr = String(val, 1);
        }
        epaper.drawString(valueStr, x + 15, y + 85);
        
        // ----- Unit | 单位 -----
        if (entity->unit.length() > 0) {
            int valueWidth = valueStr.length() * 30;
            epaper.setTextSize(2);
            epaper.setTextColor(COLOR_DARK);
            epaper.drawString(entity->unit, x + 20 + valueWidth, y + 105);
        }
        
        // Status indicator | 状态指示器
        epaper.fillCircle(x + width - 20, y + height - 20, 6, COLOR_BLACK);
        
    } else {
        // ----- Empty Card | 空卡片 -----
        // Plus sign | 加号
        int centerX = x + width / 2;
        int centerY = y + height / 2;
        epaper.fillRect(centerX - 20, centerY - 3, 40, 6, COLOR_LIGHT);
        epaper.fillRect(centerX - 3, centerY - 20, 6, 40, COLOR_LIGHT);
        
        // Hint text | 提示文字
        epaper.setTextColor(COLOR_DARK);
        epaper.setTextSize(1);
        epaper.drawString("Subscribe in HA", x + width/2 - 50, y + height - 25);
        
        // Empty status indicator | 空状态指示器
        epaper.drawCircle(x + width - 20, y + height - 20, 6, COLOR_DARK);
    }
#endif
}

/**
 * ★★★ MAIN CUSTOM DASHBOARD FUNCTION ★★★
 * ★★★ 主要的自定义仪表板函数 ★★★
 * 
 * This function is called when the display needs to refresh.
 * 当显示需要刷新时调用此函数。
 * 
 * You have full control over what to draw on the 800x480 screen!
 * 你可以完全控制在 800x480 屏幕上绘制什么！
 * 
 * Available drawing functions (from TFT_eSPI):
 * 可用的绘图函数（来自 TFT_eSPI）：
 * - epaper.fillScreen(color)                    - Fill entire screen | 填充整个屏幕
 * - epaper.fillRect(x, y, w, h, color)          - Filled rectangle | 填充矩形
 * - epaper.drawRect(x, y, w, h, color)          - Rectangle outline | 矩形边框
 * - epaper.drawLine(x1, y1, x2, y2, color)      - Draw line | 画线
 * - epaper.fillCircle(x, y, r, color)           - Filled circle | 填充圆
 * - epaper.drawCircle(x, y, r, color)           - Circle outline | 圆边框
 * - epaper.setTextSize(size)                    - Set text size (1-7) | 设置文字大小
 * - epaper.setTextColor(color)                  - Set text color | 设置文字颜色
 * - epaper.drawString(str, x, y)                - Draw string | 绘制字符串
 * - epaper.drawNumber(num, x, y)                - Draw number | 绘制数字
 */
void drawCustomDashboard() {
#ifdef EPAPER_ENABLE
    // =========================================================================
    // Clear screen | 清屏
    // =========================================================================
    epaper.fillScreen(COLOR_WHITE);
    
    // =========================================================================
    // HEADER BAR | 顶部标题栏
    // =========================================================================
    int headerHeight = 70;
    
    // Header background with gradient | 带渐变的标题栏背景
    epaper.fillRect(0, 0, SCREEN_WIDTH, headerHeight - 10, COLOR_BLACK);
    epaper.fillRect(0, headerHeight - 10, SCREEN_WIDTH, 5, COLOR_DARK);
    epaper.fillRect(0, headerHeight - 5, SCREEN_WIDTH, 5, COLOR_LIGHT);
    
    // Title | 标题
    epaper.setTextColor(COLOR_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("Home Assistant Dashboard", 30, 20);
    
    // Connection status | 连接状态
    epaper.setTextSize(2);
    String status = isHAOnline() ? "Online" : "Offline";
    epaper.drawString(status, SCREEN_WIDTH - 120, 25);
    
    // Status indicator dot | 状态指示点
    if (isHAOnline()) {
        epaper.fillCircle(SCREEN_WIDTH - 140, 32, 8, COLOR_WHITE);
    } else {
        epaper.drawCircle(SCREEN_WIDTH - 140, 32, 8, COLOR_WHITE);
    }
    
    // =========================================================================
    // MAIN CONTENT AREA - Entity Cards | 主内容区域 - 实体卡片
    // =========================================================================
    // 
    // ★ TWO WAYS TO DISPLAY ENTITIES | 两种显示实体的方式 ★
    //
    // WAY 1: Dynamic Layout (current implementation)
    // 方式1：动态布局（当前实现）
    // - Loops through all subscribed entities
    // - 遍历所有订阅的实体
    // - Order depends on HA subscription order
    // - 顺序取决于 HA 订阅顺序
    // - Good for: showing all entities automatically
    // - 适用于：自动显示所有实体
    //
    // WAY 2: Fixed Layout (uncomment below to use)
    // 方式2：固定布局（取消下面的注释使用）
    // - You specify which entity goes where
    // - 你指定哪个实体显示在哪里
    // - Uses getEntityById("sensor.xxx")
    // - 使用 getEntityById("sensor.xxx")
    // - Good for: custom dashboards with specific layouts
    // - 适用于：有特定布局需求的自定义仪表板
    //
    // =========================================================================
    
    const int cardWidth = 240;
    const int cardHeight = 150;
    const int cardMargin = 20;
    const int startX = (SCREEN_WIDTH - (3 * cardWidth + 2 * cardMargin)) / 2;
    const int startY = headerHeight + 20;
    
    // -------------------------------------------------------------------------
    // WAY 1: Dynamic Layout - loops through all entities
    // 方式1：动态布局 - 遍历所有实体
    // -------------------------------------------------------------------------
    int cardIndex = 0;
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            int x = startX + col * (cardWidth + cardMargin);
            int y = startY + row * (cardHeight + cardMargin);
            
            // Get entity for this card position | 获取此卡片位置的实体
            EntityData* entity = (cardIndex < getEntityCount()) ? getEntity(cardIndex) : NULL;
            
            // Draw the card | 绘制卡片
            drawEntityCard(x, y, cardWidth, cardHeight, entity);
            
            cardIndex++;
        }
    }
    
    // -------------------------------------------------------------------------
    // WAY 2: Fixed Layout - specify exact entities for each position
    // 方式2：固定布局 - 为每个位置指定确切的实体
    // -------------------------------------------------------------------------
    // Uncomment and modify the entity IDs to match your HA entities:
    // 取消注释并修改实体ID以匹配你的 HA 实体：
    //
    // // Row 1 | 第1行
    // drawEntityCard(startX + 0*(cardWidth+cardMargin), startY, cardWidth, cardHeight,
    //                getEntityById("sensor.living_room_temperature"));
    // drawEntityCard(startX + 1*(cardWidth+cardMargin), startY, cardWidth, cardHeight,
    //                getEntityById("sensor.living_room_humidity"));
    // drawEntityCard(startX + 2*(cardWidth+cardMargin), startY, cardWidth, cardHeight,
    //                getEntityById("sensor.outdoor_temperature"));
    // 
    // // Row 2 | 第2行
    // drawEntityCard(startX + 0*(cardWidth+cardMargin), startY + cardHeight + cardMargin, cardWidth, cardHeight,
    //                getEntityById("sensor.power_consumption"));
    // drawEntityCard(startX + 1*(cardWidth+cardMargin), startY + cardHeight + cardMargin, cardWidth, cardHeight,
    //                getEntityById("sensor.phone_battery"));
    // drawEntityCard(startX + 2*(cardWidth+cardMargin), startY + cardHeight + cardMargin, cardWidth, cardHeight,
    //                getEntityById("sensor.air_quality"));
    
    // =========================================================================
    // FOOTER BAR | 底部信息栏
    // =========================================================================
    int footerY = SCREEN_HEIGHT - 40;
    
    epaper.fillRect(0, footerY, SCREEN_WIDTH, 40, COLOR_LIGHT);
    epaper.drawLine(0, footerY, SCREEN_WIDTH, footerY, COLOR_DARK);
    
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(1);
    
    // Device info | 设备信息
    epaper.drawString("Device: " + getDeviceId(), 20, footerY + 12);
    epaper.drawString("IP: " + getDeviceIP(), 280, footerY + 12);
    
    // Uptime | 运行时间
    unsigned long uptime = millis() / 1000;
    epaper.drawString("Uptime: " + String(uptime / 60) + "m", 500, footerY + 12);
    
    // Entity count | 实体数量
    epaper.drawString("Entities: " + String(getEntityCount()), SCREEN_WIDTH - 120, footerY + 12);
#endif
}

/**
 * Draw WiFi provisioning screen
 * 绘制 WiFi 配网界面
 * 
 * Called when device enters AP mode for WiFi configuration.
 * 当设备进入 AP 模式进行 WiFi 配置时调用。
 */
void drawProvisioningScreen() {
#ifdef EPAPER_ENABLE
    epaper.fillScreen(COLOR_WHITE);
    
    int centerX = SCREEN_WIDTH / 2;
    
    // Header | 标题
    epaper.fillRect(0, 0, SCREEN_WIDTH, 80, COLOR_BLACK);
    epaper.setTextColor(COLOR_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("WiFi Setup Required", centerX - 180, 25);
    
    // WiFi icon | WiFi 图标
    int iconY = 150;
    epaper.drawCircle(centerX, iconY + 30, 60, COLOR_BLACK);
    epaper.drawCircle(centerX, iconY + 30, 45, COLOR_DARK);
    epaper.drawCircle(centerX, iconY + 30, 30, COLOR_BLACK);
    epaper.fillCircle(centerX, iconY + 30, 12, COLOR_BLACK);
    epaper.fillRect(centerX - 70, iconY + 30, 140, 70, COLOR_WHITE);
    
    // Instructions box | 说明框
    int boxY = 220;
    int boxHeight = 180;
    epaper.fillRect(50, boxY, SCREEN_WIDTH - 100, boxHeight, COLOR_LIGHT);
    epaper.drawRect(50, boxY, SCREEN_WIDTH - 100, boxHeight, COLOR_BLACK);
    
    // Step 1 | 步骤 1
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(2);
    epaper.drawString("Step 1: Connect to WiFi hotspot", 80, boxY + 20);
    epaper.setTextSize(3);
    epaper.drawString(AP_SSID, 120, boxY + 50);
    
    // Separator | 分隔线
    epaper.drawLine(80, boxY + 90, SCREEN_WIDTH - 80, boxY + 90, COLOR_DARK);
    
    // Step 2 | 步骤 2
    epaper.setTextSize(2);
    epaper.drawString("Step 2: Open browser and visit", 80, boxY + 105);
    epaper.setTextSize(3);
    epaper.drawString("http://192.168.4.1", 120, boxY + 135);
    
    // Bottom text | 底部文字
    epaper.setTextColor(COLOR_DARK);
    epaper.setTextSize(2);
    epaper.drawString("Select your WiFi network and enter password", centerX - 230, boxY + boxHeight + 20);
    
    epaper.update();
#endif
}

/**
 * Draw WiFi connected screen
 * 绘制 WiFi 已连接界面
 * 
 * Called after successful WiFi connection.
 * 成功连接 WiFi 后调用。
 */
void drawConnectedScreen(const char* ip) {
#ifdef EPAPER_ENABLE
    epaper.fillScreen(COLOR_WHITE);
    
    int centerX = SCREEN_WIDTH / 2;
    
    // Header | 标题
    epaper.fillRect(0, 0, SCREEN_WIDTH, 80, COLOR_BLACK);
    epaper.setTextColor(COLOR_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("WiFi Connected!", centerX - 130, 25);
    
    // Checkmark | 对勾
    int checkY = 160;
    epaper.fillCircle(centerX, checkY, 50, COLOR_BLACK);
    epaper.drawLine(centerX - 25, checkY, centerX - 5, checkY + 20, COLOR_WHITE);
    epaper.drawLine(centerX - 24, checkY, centerX - 4, checkY + 20, COLOR_WHITE);
    epaper.drawLine(centerX - 5, checkY + 20, centerX + 30, checkY - 20, COLOR_WHITE);
    epaper.drawLine(centerX - 4, checkY + 20, centerX + 31, checkY - 20, COLOR_WHITE);
    
    // IP address | IP 地址
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(2);
    epaper.drawString("Device IP Address:", centerX - 100, 240);
    epaper.setTextSize(4);
    epaper.drawString(ip, centerX - 140, 280);
    
    // Instructions | 说明
    int boxY = 350;
    epaper.fillRect(100, boxY, SCREEN_WIDTH - 200, 80, COLOR_LIGHT);
    epaper.drawRect(100, boxY, SCREEN_WIDTH - 200, 80, COLOR_DARK);
    
    epaper.setTextSize(2);
    epaper.drawString("Add this device in Home Assistant:", centerX - 180, boxY + 15);
    epaper.drawString("Settings -> Devices & Services", centerX - 160, boxY + 45);
    
    // Footer | 底部
    epaper.setTextColor(COLOR_DARK);
    epaper.setTextSize(1);
    epaper.drawString("Waiting for Home Assistant connection...", centerX - 130, SCREEN_HEIGHT - 30);
    
    epaper.update();
#endif
}

/**
 * Draw startup/initializing screen
 * 绘制启动/初始化界面
 */
void drawStartupScreen(const char* status) {
#ifdef EPAPER_ENABLE
    epaper.fillScreen(COLOR_WHITE);
    
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2 - 40;
    
    // Decorative circles | 装饰圆圈
    epaper.fillCircle(centerX, centerY - 60, 70, COLOR_LIGHT);
    epaper.fillCircle(centerX, centerY - 60, 60, COLOR_DARK);
    epaper.fillCircle(centerX, centerY - 60, 50, COLOR_BLACK);
    
    // HA text | HA 文字
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
    
    epaper.setTextColor(COLOR_BLACK);
    epaper.setTextSize(2);
    epaper.drawString(status, centerX - 150, statusBoxY + 10);
    
    // Grayscale demo bars | 灰度演示条
    int barWidth = SCREEN_WIDTH / 4;
    epaper.fillRect(0 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_BLACK);
    epaper.fillRect(1 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_DARK);
    epaper.fillRect(2 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_LIGHT);
    epaper.fillRect(3 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_WHITE);
    epaper.drawRect(3 * barWidth, SCREEN_HEIGHT - 25, barWidth, 25, COLOR_BLACK);
    
    epaper.update();
#endif
}


// *****************************************************************************
// *                                                                           *
// *    ███████╗██████╗  █████╗ ███╗   ███╗███████╗██╗    ██╗ ██████╗ ██████╗  *
// *    ██╔════╝██╔══██╗██╔══██╗████╗ ████║██╔════╝██║    ██║██╔═══██╗██╔══██╗ *
// *    █████╗  ██████╔╝███████║██╔████╔██║█████╗  ██║ █╗ ██║██║   ██║██████╔╝ *
// *    ██╔══╝  ██╔══██╗██╔══██║██║╚██╔╝██║██╔══╝  ██║███╗██║██║   ██║██╔══██╗ *
// *    ██║     ██║  ██║██║  ██║██║ ╚═╝ ██║███████╗╚███╔███╔╝╚██████╔╝██║  ██║ *
// *    ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝ ╚══╝╚══╝  ╚═════╝ ╚═╝  ╚═╝ *
// *                                                                           *
// *      SECTION 3: FRAMEWORK CODE | 第3部分：框架代码                          *
// *                                                                           *
// *      ⚠️  DO NOT MODIFY BELOW THIS LINE  ⚠️                                *
// *      ⚠️  请勿修改以下内容  ⚠️                                              *
// *                                                                           *
// *****************************************************************************

// =============================================================================
// Hardware Pin Definitions | 硬件引脚定义
// =============================================================================
#define PIN_RESET_BUTTON 3
#define PIN_STATUS_LED 6
#define PIN_BUZZER 45
#define WIFI_RESET_HOLD_TIME 6000
#define SERIAL_RX 44
#define SERIAL_TX 43

// =============================================================================
// Framework Global Variables | 框架全局变量
// =============================================================================
unsigned long _lastDisplayUpdate = 0;
bool _initialRefreshDone = false;
const unsigned long _DATA_COLLECTION_WAIT = 5000;
unsigned long _dataReceivedTime = 0;
bool _dataReceived = false;
int _lastEntityCount = 0;

bool _configChanged = false;
unsigned long _configChangeTime = 0;

bool _lastHAConnected = false;
unsigned long _haStatusChangeTime = 0;
bool _haStatusPendingRefresh = false;
const unsigned long _HA_STATUS_DEBOUNCE = 30000;

bool _wifiProvisioningMode = false;

volatile uint32_t _resetButtonPressTime = 0;
volatile bool _resetButtonPressed = false;
volatile bool _resetFeedbackGiven = false;
volatile bool _wifiResetRequested = false;
TaskHandle_t _resetButtonTaskHandle = NULL;

// =============================================================================
// Public API Functions for User Code | 用户代码的公共 API 函数
// =============================================================================

/**
 * Get entity by index | 按索引获取实体
 * @param index Entity index (0 to MAX_ENTITIES-1)
 * @return Pointer to entity data, or NULL if index out of range
 * 
 * Use this for looping through all entities:
 * 用于遍历所有实体：
 *   for (int i = 0; i < getEntityCount(); i++) {
 *       EntityData* e = getEntity(i);
 *       // e->entityId tells you which entity this is
 *   }
 */
EntityData* getEntity(int index) {
    if (index >= 0 && index < _entityCount) {
        return &_entities[index];
    }
    return NULL;
}

/**
 * ★ RECOMMENDED ★ Get entity by its ID | ★ 推荐 ★ 按实体ID获取
 * @param entityId The entity ID from Home Assistant (e.g., "sensor.temperature")
 *                 来自 Home Assistant 的实体ID（如 "sensor.temperature"）
 * @return Pointer to entity data, or NULL if not found/not subscribed
 *         返回实体数据指针，如果未找到/未订阅则返回 NULL
 * 
 * Example | 示例:
 *   EntityData* temp = getEntityById("sensor.living_room_temperature");
 *   EntityData* hum = getEntityById("sensor.living_room_humidity");
 *   
 *   if (temp != NULL && temp->hasValue) {
 *       drawValue(temp->state, temp->unit);  // "23.5", "°C"
 *   }
 * 
 * Note: The entityId must match exactly what you subscribed in Home Assistant
 * 注意：entityId 必须与你在 Home Assistant 中订阅的完全一致
 */
EntityData* getEntityById(const char* entityId) {
    for (int i = 0; i < _entityCount; i++) {
        if (_entities[i].entityId == entityId) {
            return &_entities[i];
        }
    }
    return NULL;
}

// Overload for String parameter | String 参数重载
EntityData* getEntityById(const String& entityId) {
    return getEntityById(entityId.c_str());
}

/**
 * Get total number of subscribed entities | 获取订阅的实体总数
 */
int getEntityCount() {
    return _entityCount;
}

/**
 * Check if connected to Home Assistant | 检查是否已连接到 Home Assistant
 */
bool isHAOnline() {
    return ha.isHAConnected();
}

/**
 * Get device IP address as string | 获取设备 IP 地址字符串
 */
String getDeviceIP() {
    return ha.getLocalIP().toString();
}

/**
 * Get device unique ID | 获取设备唯一 ID
 */
String getDeviceId() {
    return ha.getDeviceId();
}

// =============================================================================
// Framework Internal Functions | 框架内部函数
// =============================================================================

void _setStatusLED(bool on) {
    digitalWrite(PIN_STATUS_LED, on ? LOW : HIGH);
}

void _resetButtonTask(void* parameter) {
    Serial1.println("[RTOS] Reset button task started on Core " + String(xPortGetCoreID()));
    
    while (true) {
        bool currentState = (digitalRead(PIN_RESET_BUTTON) == LOW);
        uint32_t now = millis();
        
        if (currentState && !_resetButtonPressed) {
            _resetButtonPressed = true;
            _resetButtonPressTime = now;
            _resetFeedbackGiven = false;
            Serial1.println("Reset button pressed...");
        }
        
        if (currentState && _resetButtonPressed && !_resetFeedbackGiven) {
            uint32_t holdTime = now - _resetButtonPressTime;
            
            if (holdTime >= WIFI_RESET_HOLD_TIME) {
                _resetFeedbackGiven = true;
                Serial1.println("\n=========================================");
                Serial1.println("  WiFi Reset threshold reached (6s)!");
                Serial1.println("  Release button to reset WiFi...");
                Serial1.println("=========================================");
                
                for (int i = 0; i < 3; i++) {
                    tone(PIN_BUZZER, 1500);
                    _setStatusLED(true);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    noTone(PIN_BUZZER);
                    tone(PIN_BUZZER, 1000);
                    _setStatusLED(false);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    noTone(PIN_BUZZER);
                }
                _setStatusLED(true);
                tone(PIN_BUZZER, 2000);
                vTaskDelay(pdMS_TO_TICKS(200));
                noTone(PIN_BUZZER);
            }
        }
        
        if (!currentState && _resetButtonPressed) {
            uint32_t holdTime = now - _resetButtonPressTime;
            _resetButtonPressed = false;
            
            if (_resetFeedbackGiven && holdTime >= WIFI_RESET_HOLD_TIME) {
                Serial1.println("\n=========================================");
                Serial1.println("  WiFi Reset triggered!");
                Serial1.println("=========================================");
                
                tone(PIN_BUZZER, 800);
                vTaskDelay(pdMS_TO_TICKS(500));
                noTone(PIN_BUZZER);
                _setStatusLED(false);
                
                _wifiResetRequested = true;
            }
            
            _setStatusLED(false);
            _resetFeedbackGiven = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void _updateEntitiesFromHA() {
    const auto& states = ha.getHAStates();
    _entityCount = 0;
    
    for (const auto& pair : states) {
        if (_entityCount >= MAX_ENTITIES) break;
        
        SeeedHAState* state = pair.second;
        EntityData& entity = _entities[_entityCount];
        
        entity.entityId = state->getEntityId();
        entity.friendlyName = state->getFriendlyName();
        entity.state = state->getString();
        entity.unit = state->getUnit();
        entity.deviceClass = state->getDeviceClass();
        entity.hasValue = state->hasValue();
        
        _entityCount++;
    }
    
    Serial1.printf("Updated %d entities for display\n", _entityCount);
}

void _refreshDisplay() {
    Serial1.println("Drawing dashboard...");
    ha.handle();
    
    _updateEntitiesFromHA();
    drawCustomDashboard();  // Call user's custom drawing function
    
    ha.handle();
    
    Serial1.println("Updating E-Paper display (this may take a while)...");
#ifdef EPAPER_ENABLE
    epaper.update();
#endif
    Serial1.println("Display updated!");
    
    for (int i = 0; i < 5; i++) {
        ha.handle();
        delay(10);
    }
}

// =============================================================================
// Arduino Setup & Loop | Arduino 设置与主循环
// =============================================================================

void setup() {
    Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
    delay(1000);
    
    Serial1.println();
    Serial1.println("============================================");
    Serial1.println("  " DEVICE_MODEL " - HA Display Dashboard");
    Serial1.println("  (4-Level Grayscale E-Paper)");
    Serial1.println("============================================");
    
#ifdef EPAPER_ENABLE
    Serial1.println("Initializing E-Paper display...");
    epaper.begin();
    epaper.fillScreen(TFT_WHITE);
    epaper.update();
    
    Serial1.println("Initializing grayscale mode...");
    epaper.initGrayMode(GRAY_LEVEL4);
    
    Serial1.println("Displaying startup screen...");
    drawStartupScreen("Initializing...");
#else
    Serial1.println("WARNING: EPAPER_ENABLE not defined!");
#endif
    
    // Initialize GPIO
    Serial1.println("Initializing GPIO pins...");
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
    
    // Create reset button task
    xTaskCreatePinnedToCore(
        _resetButtonTask, "ResetButton", 4096, NULL, 1,
        &_resetButtonTaskHandle, 0
    );
    
    // LED blink to indicate boot
    for (int i = 0; i < 2; i++) {
        _setStatusLED(true); delay(100);
        _setStatusLED(false); delay(100);
    }
    
    ha.enableDebug(true);
    ha.setDeviceInfo(DEVICE_NAME, DEVICE_MODEL, FIRMWARE_VERSION);
    
    // Register HA state callback
    ha.onHAState([](const char* entityId, const char* state, JsonObject& attrs) {
        Serial1.printf("HA State received: %s = %s\n", entityId, state);
        
        int currentCount = ha.getHAStates().size();
        
        if (!_initialRefreshDone) {
            if (!_dataReceived) {
                _dataReceived = true;
                _dataReceivedTime = millis();
                Serial1.println("First data received, waiting for more entities...");
            }
            if (currentCount > _lastEntityCount) {
                _lastEntityCount = currentCount;
                _dataReceivedTime = millis();
                Serial1.printf("Entity count: %d, resetting wait timer...\n", currentCount);
            }
        } else if (_configChanged) {
            if (currentCount > _lastEntityCount) {
                _lastEntityCount = currentCount;
                _configChangeTime = millis();
                Serial1.printf("Config update: %d entities, resetting wait timer...\n", currentCount);
            }
        }
    });
    
    // Connect WiFi
    Serial1.println();
    
#if USE_WIFI_PROVISIONING
    Serial1.println("Starting with WiFi provisioning...");
    Serial1.print("  AP Name (if needed): ");
    Serial1.println(AP_SSID);
    
    bool wifiConnected = ha.beginWithProvisioning(AP_SSID);
    ha.enableResetButton(PIN_RESET_BUTTON);
    
    if (!wifiConnected) {
        Serial1.println("\n============================================");
        Serial1.println("  WiFi Provisioning Mode Active!");
        Serial1.println("============================================");
        Serial1.println("  1. Connect to WiFi: " + String(AP_SSID));
        Serial1.println("  2. Open browser: http://192.168.4.1");
        Serial1.println("  Long press GPIO3 (6s) to reset WiFi credentials\n");
        
        _wifiProvisioningMode = true;
        
        for (int i = 0; i < 3; i++) {
            _setStatusLED(true); delay(300);
            _setStatusLED(false); delay(300);
        }
        
#ifdef EPAPER_ENABLE
        drawProvisioningScreen();
#endif
        return;
    }
#else
    Serial1.println("Connecting to WiFi...");
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
    
    for (int i = 0; i < 3; i++) {
        _setStatusLED(true); delay(100);
        _setStatusLED(false); delay(100);
    }
    
#ifdef EPAPER_ENABLE
    drawConnectedScreen(ha.getLocalIP().toString().c_str());
#endif
    
    Serial1.println("\n============================================");
    Serial1.println("  Setup Complete!");
    Serial1.println("============================================\n");
    Serial1.println("Configure entity subscriptions in HA:");
    Serial1.println("1. Settings > Devices & Services");
    Serial1.println("2. Find this device > Configure");
    Serial1.println("3. Select entities to subscribe\n");
}

void loop() {
    ha.handle();
    
    // Handle WiFi reset request
    if (_wifiResetRequested) {
        _wifiResetRequested = false;
        Serial1.println("  Clearing credentials and restarting...");
        ha.clearWiFiCredentials();
        Serial1.flush();
        delay(500);
        ESP.restart();
    }
    
    // Detect AP mode entry
    if (!_wifiProvisioningMode && ha.isProvisioningActive()) {
        Serial1.println("\n============================================");
        Serial1.println("  Entered AP Mode (WiFi Reset Triggered)!");
        Serial1.println("============================================");
        
        _wifiProvisioningMode = true;
        _initialRefreshDone = false;
        _dataReceived = false;
        _lastEntityCount = 0;
        
#ifdef EPAPER_ENABLE
        drawProvisioningScreen();
#endif
    }
    
    // Provisioning mode handling
    if (_wifiProvisioningMode) {
        if (ha.isWiFiConnected()) {
            Serial1.println("\n============================================");
            Serial1.println("  WiFi Connected via Provisioning!");
            Serial1.println("============================================");
            Serial1.print("IP: ");
            Serial1.println(ha.getLocalIP());
            
            _wifiProvisioningMode = false;
            
            for (int i = 0; i < 5; i++) {
                _setStatusLED(true); delay(100);
                _setStatusLED(false); delay(100);
            }
            
#ifdef EPAPER_ENABLE
            drawConnectedScreen(ha.getLocalIP().toString().c_str());
#endif
        }
        
        static unsigned long lastProvStatus = 0;
        if (millis() - lastProvStatus > 30000) {
            lastProvStatus = millis();
            Serial1.println("Status: WiFi Provisioning mode active...");
        }
        return;
    }
    
    unsigned long now = millis();
    int currentEntityCount = ha.getHAStates().size();
    
    // Detect config clear
    if (_initialRefreshDone && !_configChanged && _lastEntityCount > 0 && currentEntityCount == 0) {
        Serial1.println("Config cleared! Waiting for new entities...");
        _configChanged = true;
        _configChangeTime = now;
        _lastEntityCount = 0;
    }
    
    // Refresh logic
    bool shouldRefresh = false;
    
    // HA connection status change with debounce
    bool currentHAConnected = ha.isHAConnected();
    if (_initialRefreshDone && _lastHAConnected != currentHAConnected) {
        if (!_haStatusPendingRefresh) {
            _haStatusPendingRefresh = true;
            _haStatusChangeTime = now;
            Serial1.println(currentHAConnected ? 
                "HA connected! Waiting for stable connection..." :
                "HA disconnected! Waiting to confirm...");
        }
    }
    
    if (_haStatusPendingRefresh && (now - _haStatusChangeTime >= _HA_STATUS_DEBOUNCE)) {
        if (currentHAConnected != _lastHAConnected) {
            Serial1.println(currentHAConnected ? 
                "HA connection stable! Refreshing display..." :
                "HA disconnection confirmed! Refreshing display...");
            shouldRefresh = true;
            _lastDisplayUpdate = now;
            _lastHAConnected = currentHAConnected;
        }
        _haStatusPendingRefresh = false;
    } else if (_haStatusPendingRefresh && currentHAConnected == _lastHAConnected) {
        Serial1.println("HA status reverted, canceling refresh.");
        _haStatusPendingRefresh = false;
    }
    
    // Initial refresh
    if (!shouldRefresh && !_initialRefreshDone && _dataReceived) {
        if (now - _dataReceivedTime >= _DATA_COLLECTION_WAIT) {
            Serial1.printf("Data collection complete! %d entities.\n", currentEntityCount);
            shouldRefresh = true;
            _initialRefreshDone = true;
            _lastDisplayUpdate = now;
            _lastEntityCount = currentEntityCount;
        }
    }
    // Config change refresh
    else if (!shouldRefresh && _configChanged && currentEntityCount > 0) {
        if (now - _configChangeTime >= _DATA_COLLECTION_WAIT) {
            Serial1.printf("Config update complete! %d entities.\n", currentEntityCount);
            shouldRefresh = true;
            _configChanged = false;
            _lastDisplayUpdate = now;
            _lastEntityCount = currentEntityCount;
        }
    }
    // Periodic refresh
    else if (!shouldRefresh && _initialRefreshDone && !_configChanged && 
             (now - _lastDisplayUpdate >= DISPLAY_REFRESH_INTERVAL)) {
        Serial1.println("Periodic refresh triggered...");
        shouldRefresh = true;
        _lastDisplayUpdate = now;
    }
    
    if (shouldRefresh) {
        _refreshDisplay();
    }
    
    // Periodic status print
    static unsigned long lastStatusPrint = 0;
    if (now - lastStatusPrint > 30000) {
        lastStatusPrint = now;
        if (_configChanged) {
            Serial1.printf("Status: Config updating... %d entities\n", currentEntityCount);
        } else if (_initialRefreshDone) {
            Serial1.printf("Status: HA %s, Entities: %d, Next refresh in: %lu sec\n",
                ha.isHAConnected() ? "Connected" : "Waiting",
                _entityCount,
                (DISPLAY_REFRESH_INTERVAL - (now - _lastDisplayUpdate)) / 1000);
        } else if (_dataReceived) {
            Serial1.printf("Status: Collecting data... %d entities\n", currentEntityCount);
        } else {
            Serial1.println("Status: Waiting for HA data...");
        }
    }
}
