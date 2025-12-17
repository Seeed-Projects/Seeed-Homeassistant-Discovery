/**
 * ============================================================================
 * reTerminal E1002 - HA State Subscribe with E-Paper Display (6-Color)
 * reTerminal E1002 - HA 状态订阅墨水屏显示示例（六色）
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
 *   entity->color           // Color based on device class | 基于设备类型的颜色
 *   entity->hasValue        // true if data received | 如果收到数据则为true
 *
 * - Status APIs | 状态API:
 *   isHAOnline()            // true if connected to HA | 如果连接到HA则为true
 *   getDeviceIP()           // Device IP address | 设备IP地址
 *   getDeviceId()           // Device ID | 设备ID
 *
 * - Display Colors (6 colors) | 显示颜色（6色）:
 *   TFT_WHITE, TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW
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
 * - reTerminal E1002 with 6-color E-Paper display (800x480)
 *   reTerminal E1002 带六色墨水屏（800x480）
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
#define DEVICE_NAME "reTerminal_E1002"

// Device model | 设备型号
#define DEVICE_MODEL "reTerminal E1002"

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
const char* AP_SSID = "reTerminal_E1002_AP";

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
    uint16_t color;       // Color based on device class | 基于设备类型的颜色
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

// Layout constants | 布局常量
const int HEADER_HEIGHT = 60;
const int CARD_WIDTH = 240;
const int CARD_HEIGHT = 140;
const int CARD_MARGIN = 20;
const int CARD_PADDING = 15;

/**
 * Get color based on device class | 根据设备类别获取颜色
 * 6 colors available: WHITE, BLACK, RED, GREEN, BLUE, YELLOW
 * 6色可用：白、黑、红、绿、蓝、黄
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
 * Draw a single entity card | 绘制单个实体卡片
 * 
 * Example implementation - modify as you like!
 * 示例实现 - 随意修改！
 * 
 * @param x      Card X position | 卡片X坐标
 * @param y      Card Y position | 卡片Y坐标
 * @param entity Entity data pointer (can be NULL if no entity) | 实体数据指针
 */
void drawEntityCard(int x, int y, EntityData* entity) {
#ifdef EPAPER_ENABLE
    // Card background (white) | 卡片背景（白色）
    epaper.fillRect(x, y, CARD_WIDTH, CARD_HEIGHT, TFT_WHITE);
    
    // Card border | 卡片边框
    epaper.drawRect(x, y, CARD_WIDTH, CARD_HEIGHT, TFT_BLACK);
    epaper.drawRect(x + 1, y + 1, CARD_WIDTH - 2, CARD_HEIGHT - 2, TFT_BLACK);
    
    if (entity != NULL && entity->hasValue) {
        // Color accent bar on left | 左侧颜色条
        epaper.fillRect(x, y, 8, CARD_HEIGHT, entity->color);
        
        // Entity name | 实体名称
        epaper.setTextColor(TFT_BLACK);
        epaper.setTextSize(2);
        
        String displayName = entity->friendlyName;
        if (displayName.length() == 0) {
            displayName = entity->entityId;
        }
        // Truncate long names | 截断过长的名称
        if (displayName.length() > 18) {
            displayName = displayName.substring(0, 15) + "...";
        }
        epaper.drawString(displayName, x + CARD_PADDING + 5, y + CARD_PADDING);
        
        // Value | 数值
        epaper.setTextColor(entity->color);
        epaper.setTextSize(4);
        
        String valueStr = entity->state;
        // Truncate decimals if too long | 如果太长则截断小数
        if (valueStr.length() > 6) {
            float val = valueStr.toFloat();
            valueStr = String(val, 1);
        }
        
        epaper.drawString(valueStr, x + CARD_PADDING + 5, y + 50);
        
        // Unit | 单位
        if (entity->unit.length() > 0) {
            epaper.setTextSize(2);
            epaper.setTextColor(TFT_BLACK);
            int valueWidth = valueStr.length() * 24;  // Approximate width
            epaper.drawString(entity->unit, x + CARD_PADDING + 10 + valueWidth, y + 65);
        }
        
        // Status indicator (green dot) | 状态指示（绿点）
        epaper.fillCircle(x + CARD_WIDTH - 15, y + CARD_HEIGHT - 15, 5, TFT_GREEN);
        
    } else {
        // ----- Empty Card | 空卡片 -----
        // Plus sign | 加号
        int centerX = x + CARD_WIDTH / 2;
        int centerY = y + CARD_HEIGHT / 2;
        
        epaper.setTextColor(TFT_BLACK);
        epaper.setTextSize(3);
        epaper.drawString("+", centerX - 10, centerY - 15);
        
        // Hint text | 提示文字
        epaper.setTextSize(1);
        epaper.drawString("Subscribe in HA", x + 60, y + CARD_HEIGHT - 25);
        
        // Yellow waiting indicator | 黄色等待指示
        epaper.fillCircle(x + CARD_WIDTH - 15, y + CARD_HEIGHT - 15, 5, TFT_YELLOW);
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
    epaper.fillScreen(TFT_WHITE);
    
    // =========================================================================
    // HEADER BAR (Colorful) | 顶部标题栏（彩色）
    // =========================================================================
    epaper.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_BLUE);
    
    // Title text | 标题文字
    epaper.setTextColor(TFT_WHITE);
    epaper.setTextSize(3);
    epaper.drawString("Home Assistant Dashboard", 20, 18);
    
    // Connection status | 连接状态
    String status = isHAOnline() ? "Connected" : "Waiting...";
    epaper.setTextSize(2);
    epaper.drawString(status, SCREEN_WIDTH - 150, 22);
    
    // Status indicator circle | 状态指示圆点
    uint16_t statusColor = isHAOnline() ? TFT_GREEN : TFT_YELLOW;
    epaper.fillCircle(SCREEN_WIDTH - 170, 30, 8, statusColor);
    
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
    //
    // WAY 2: Fixed Layout (uncomment below to use)
    // 方式2：固定布局（取消下面的注释使用）
    // - Uses getEntityById("sensor.xxx")
    // - 使用 getEntityById("sensor.xxx")
    //
    // =========================================================================
    
    // Calculate card layout | 计算卡片布局
    // 3 columns x 2 rows = 6 cards | 3列 x 2行 = 6张卡片
    int startX = (SCREEN_WIDTH - (3 * CARD_WIDTH + 2 * CARD_MARGIN)) / 2;
    int startY = HEADER_HEIGHT + 30;
    
    // -------------------------------------------------------------------------
    // WAY 1: Dynamic Layout - loops through all entities
    // 方式1：动态布局 - 遍历所有实体
    // -------------------------------------------------------------------------
    int cardIndex = 0;
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            int x = startX + col * (CARD_WIDTH + CARD_MARGIN);
            int y = startY + row * (CARD_HEIGHT + CARD_MARGIN);
            
            // Get entity for this card position | 获取此卡片位置的实体
            EntityData* entity = (cardIndex < getEntityCount()) ? getEntity(cardIndex) : NULL;
            
            // Draw the card | 绘制卡片
            drawEntityCard(x, y, entity);
            
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
    // drawEntityCard(startX + 0*(CARD_WIDTH+CARD_MARGIN), startY,
    //                getEntityById("sensor.living_room_temperature"));
    // drawEntityCard(startX + 1*(CARD_WIDTH+CARD_MARGIN), startY,
    //                getEntityById("sensor.living_room_humidity"));
    // drawEntityCard(startX + 2*(CARD_WIDTH+CARD_MARGIN), startY,
    //                getEntityById("sensor.outdoor_temperature"));
    // 
    // // Row 2 | 第2行
    // drawEntityCard(startX + 0*(CARD_WIDTH+CARD_MARGIN), startY + CARD_HEIGHT + CARD_MARGIN,
    //                getEntityById("sensor.power_consumption"));
    // drawEntityCard(startX + 1*(CARD_WIDTH+CARD_MARGIN), startY + CARD_HEIGHT + CARD_MARGIN,
    //                getEntityById("sensor.phone_battery"));
    // drawEntityCard(startX + 2*(CARD_WIDTH+CARD_MARGIN), startY + CARD_HEIGHT + CARD_MARGIN,
    //                getEntityById("sensor.air_quality"));
    
    // =========================================================================
    // FOOTER BAR | 底部信息栏
    // =========================================================================
    int footerY = SCREEN_HEIGHT - 35;
    
    // Footer line | 底部分隔线
    epaper.drawLine(20, footerY - 5, SCREEN_WIDTH - 20, footerY - 5, TFT_BLACK);
    
    // Device info | 设备信息
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(1);
    
    epaper.drawString("Device: " + getDeviceId(), 20, footerY + 5);
    epaper.drawString("IP: " + getDeviceIP(), 280, footerY + 5);
    
    // Uptime | 运行时间
    unsigned long uptime = millis() / 1000;
    epaper.drawString("Uptime: " + String(uptime / 60) + "m", 500, footerY + 5);
    
    // Entity count | 实体数量
    epaper.drawString("Entities: " + String(getEntityCount()), SCREEN_WIDTH - 120, footerY + 5);
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
void drawConnectedScreen(const char* ip) {
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

/**
 * Draw startup/initializing screen
 * 绘制启动/初始化界面
 */
void drawStartupScreen(const char* status) {
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
 * @return Pointer to entity data, or NULL if not found/not subscribed
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
        entity.color = getColorForDeviceClass(entity.deviceClass);
        
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
    Serial1.println("  (6-Color E-Paper)");
    Serial1.println("============================================");
    
#ifdef EPAPER_ENABLE
    Serial1.println("Initializing E-Paper display...");
    epaper.begin();
    
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
