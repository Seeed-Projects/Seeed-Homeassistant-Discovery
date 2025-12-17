/**
 * ============================================================================
 * Seeed WiFi Provisioning - Web-based WiFi Configuration for ESP32
 * Seeed WiFi Provisioning - ESP32 网页配网模块
 * ============================================================================
 *
 * This module provides web-based WiFi configuration (captive portal):
 * 这个模块提供网页配网功能（强制门户）：
 * 
 * - If WiFi connection fails, device creates an AP hotspot
 *   如果 WiFi 连接失败，设备创建 AP 热点
 * - Users can connect to the AP and configure WiFi via web browser
 *   用户可以连接 AP 并通过浏览器配置 WiFi
 * - Credentials are saved to flash and persist across reboots
 *   凭据保存到 Flash，重启后依然有效
 * - Network scanning with refresh functionality
 *   支持网络扫描和刷新功能
 *
 * Usage:
 * 使用方法：
 * ```cpp
 * #include <SeeedWiFiProvisioning.h>
 * 
 * SeeedWiFiProvisioning provisioning;
 * 
 * void setup() {
 *     // Try to connect using saved credentials, or start AP mode
 *     // 尝试使用保存的凭据连接，否则启动 AP 模式
 *     if (!provisioning.begin()) {
 *         // AP mode is active, handle in loop
 *         // AP 模式已激活，在 loop 中处理
 *     }
 * }
 * 
 * void loop() {
 *     provisioning.handle();  // Required! | 必须调用！
 * }
 * ```
 *
 * @author limengdu
 * @version 1.0.0
 * @license MIT
 */

#ifndef SEEED_WIFI_PROVISIONING_H
#define SEEED_WIFI_PROVISIONING_H

// =============================================================================
// Dependencies | 依赖库
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// =============================================================================
// Constants | 常量定义
// =============================================================================

// Default AP settings | 默认 AP 设置
#define SEEED_WIFI_PROV_DEFAULT_AP_SSID     "Seeed_IoT_Device_AP"
#define SEEED_WIFI_PROV_DEFAULT_AP_PASSWORD ""  // Open network by default | 默认开放网络
#define SEEED_WIFI_PROV_AP_CHANNEL          1
#define SEEED_WIFI_PROV_AP_MAX_CONNECTIONS  4

// Web server settings | Web 服务器设置
#define SEEED_WIFI_PROV_HTTP_PORT           80
#define SEEED_WIFI_PROV_DNS_PORT            53

// WiFi connection settings | WiFi 连接设置
#define SEEED_WIFI_PROV_CONNECT_TIMEOUT     15000  // 15 seconds | 15 秒
#define SEEED_WIFI_PROV_CONNECT_RETRY       3      // Retry 3 times | 重试 3 次

// Reset button settings | 重置按钮设置
#define SEEED_WIFI_PROV_RESET_HOLD_TIME     6000   // Long press 6 seconds to reset | 长按 6 秒重置

// Preferences namespace | Preferences 命名空间
#define SEEED_WIFI_PROV_PREFS_NAMESPACE     "seeed_wifi"
#define SEEED_WIFI_PROV_PREFS_SSID_KEY      "ssid"
#define SEEED_WIFI_PROV_PREFS_PASS_KEY      "password"
#define SEEED_WIFI_PROV_PREFS_CONFIG_KEY    "configured"

// =============================================================================
// Callback Type Definitions | 回调函数类型定义
// =============================================================================

/**
 * WiFi connected callback type
 * WiFi 连接成功回调函数类型
 */
typedef void (*WiFiConnectedCallback)();

/**
 * WiFi connection failed callback type
 * WiFi 连接失败回调函数类型
 */
typedef void (*WiFiFailedCallback)();

/**
 * AP mode started callback type
 * AP 模式启动回调函数类型
 */
typedef void (*APStartedCallback)();

// =============================================================================
// WiFi Network Info Structure | WiFi 网络信息结构
// =============================================================================

/**
 * Structure to hold scanned WiFi network information
 * 保存扫描到的 WiFi 网络信息的结构体
 */
struct WiFiNetworkInfo {
    String ssid;
    int32_t rssi;
    uint8_t encryptionType;
    uint8_t* bssid;
    int32_t channel;
};

// =============================================================================
// SeeedWiFiProvisioning - Main Class | 主类
// =============================================================================

/**
 * Seeed WiFi Provisioning Main Class
 * Seeed WiFi Provisioning 主类
 *
 * Provides web-based WiFi configuration functionality:
 * 提供网页配网功能：
 * - Captive portal for WiFi configuration
 *   强制门户用于 WiFi 配置
 * - Network scanning and selection
 *   网络扫描和选择
 * - Credentials persistence across reboots
 *   凭据在重启后保持
 */
class SeeedWiFiProvisioning {
public:
    /**
     * Constructor | 构造函数
     */
    SeeedWiFiProvisioning();

    /**
     * Destructor | 析构函数
     */
    ~SeeedWiFiProvisioning();

    // =========================================================================
    // Configuration Methods | 配置方法
    // =========================================================================

    /**
     * Set AP SSID
     * 设置 AP SSID
     *
     * @param ssid AP hotspot name (default: "Seeed_IoT_Device_AP")
     *             AP 热点名称（默认："Seeed_IoT_Device_AP"）
     */
    void setAPSSID(const String& ssid);

    /**
     * Set AP password
     * 设置 AP 密码
     *
     * @param password AP password (empty string = open network)
     *                 AP 密码（空字符串 = 开放网络）
     */
    void setAPPassword(const String& password);

    /**
     * Set connection timeout
     * 设置连接超时时间
     *
     * @param timeout Connection timeout in milliseconds (default: 15000)
     *                连接超时时间，毫秒（默认：15000）
     */
    void setConnectTimeout(uint32_t timeout);

    /**
     * Enable debug output
     * 启用调试输出
     *
     * @param enable Whether to enable debug (default true)
     *               是否启用调试（默认 true）
     */
    void enableDebug(bool enable = true);

    // =========================================================================
    // Callback Registration | 回调注册
    // =========================================================================

    /**
     * Register callback for successful WiFi connection
     * 注册 WiFi 连接成功回调
     */
    void onWiFiConnected(WiFiConnectedCallback callback);

    /**
     * Register callback for WiFi connection failure
     * 注册 WiFi 连接失败回调
     */
    void onWiFiFailed(WiFiFailedCallback callback);

    /**
     * Register callback for AP mode started
     * 注册 AP 模式启动回调
     */
    void onAPStarted(APStartedCallback callback);

    // =========================================================================
    // Connection Methods | 连接方法
    // =========================================================================

    /**
     * Begin WiFi connection
     * 开始 WiFi 连接
     *
     * This method will:
     * 这个方法会：
     * 1. Check for saved credentials | 检查是否有保存的凭据
     * 2. If found, try to connect | 如果有，尝试连接
     * 3. If connection fails, start AP mode | 如果连接失败，启动 AP 模式
     *
     * @return true if WiFi connected successfully, false if AP mode started
     *         WiFi 连接成功返回 true，启动 AP 模式返回 false
     */
    bool begin();

    /**
     * Begin WiFi connection with specific credentials
     * 使用指定凭据开始 WiFi 连接
     *
     * If connection fails, will fall back to AP mode.
     * 如果连接失败，会回退到 AP 模式。
     *
     * @param ssid WiFi SSID | WiFi 名称
     * @param password WiFi password | WiFi 密码
     * @param saveCredentials Whether to save credentials to flash (default true)
     *                        是否将凭据保存到 Flash（默认 true）
     * @return true if WiFi connected successfully, false if AP mode started
     *         WiFi 连接成功返回 true，启动 AP 模式返回 false
     */
    bool begin(const char* ssid, const char* password, bool saveCredentials = true);

    /**
     * Force start AP mode for configuration
     * 强制启动 AP 模式进行配置
     *
     * Use this to manually enter configuration mode even when WiFi is connected.
     * 用于手动进入配置模式，即使 WiFi 已连接。
     *
     * @return true if AP mode started successfully
     *         AP 模式启动成功返回 true
     */
    bool startAPMode();

    /**
     * Stop AP mode
     * 停止 AP 模式
     */
    void stopAPMode();

    // =========================================================================
    // Runtime Methods | 运行时方法
    // =========================================================================

    /**
     * Handle provisioning tasks
     * 处理配网任务
     *
     * Must be called in loop() when AP mode is active!
     * AP 模式激活时必须在 loop() 中调用！
     */
    void handle();

    // =========================================================================
    // Status Queries | 状态查询
    // =========================================================================

    /**
     * Check if WiFi is connected
     * 检查是否已连接 WiFi
     */
    bool isWiFiConnected() const;

    /**
     * Check if AP mode is active
     * 检查 AP 模式是否激活
     */
    bool isAPModeActive() const;

    /**
     * Check if credentials are saved
     * 检查是否有保存的凭据
     */
    bool hasCredentials() const;

    /**
     * Get saved SSID
     * 获取保存的 SSID
     */
    String getSavedSSID() const;

    /**
     * Get local IP address
     * 获取本机 IP 地址
     */
    IPAddress getLocalIP() const;

    /**
     * Get AP IP address
     * 获取 AP IP 地址
     */
    IPAddress getAPIP() const;

    // =========================================================================
    // Credential Management | 凭据管理
    // =========================================================================

    /**
     * Clear saved credentials
     * 清除保存的凭据
     */
    void clearCredentials();

    /**
     * Save credentials to flash
     * 保存凭据到 Flash
     */
    void saveCredentials(const String& ssid, const String& password);

    // =========================================================================
    // Reset Button | 重置按钮
    // =========================================================================

    /**
     * Enable reset button for WiFi re-provisioning
     * 启用重置按钮，用于重新配网
     *
     * Long press the button for 6 seconds to clear credentials and restart AP mode.
     * 长按按钮 6 秒清除凭据并重启 AP 模式。
     *
     * @param pin GPIO pin number for the reset button | 重置按钮的 GPIO 引脚号
     * @param activeLow true if button is active LOW (default), false if active HIGH
     *                  按钮是否低电平有效（默认），高电平有效则为 false
     */
    void enableResetButton(int pin, bool activeLow = true);

    /**
     * Disable reset button
     * 禁用重置按钮
     */
    void disableResetButton();

    // =========================================================================
    // Network Scanning | 网络扫描
    // =========================================================================

    /**
     * Scan for WiFi networks
     * 扫描 WiFi 网络
     *
     * @return Number of networks found | 找到的网络数量
     */
    int scanNetworks();

    /**
     * Get scanned network count
     * 获取扫描到的网络数量
     */
    int getNetworkCount() const;

    /**
     * Get network SSID by index
     * 通过索引获取网络 SSID
     */
    String getNetworkSSID(int index) const;

    /**
     * Get network RSSI by index
     * 通过索引获取网络信号强度
     */
    int32_t getNetworkRSSI(int index) const;

    /**
     * Get network encryption type by index
     * 通过索引获取网络加密类型
     */
    uint8_t getNetworkEncryption(int index) const;

private:
    // -------------------------------------------------------------------------
    // Configuration | 配置
    // -------------------------------------------------------------------------
    String _apSSID;                    // AP hotspot name | AP 热点名称
    String _apPassword;                // AP password | AP 密码
    uint32_t _connectTimeout;          // Connection timeout | 连接超时
    bool _debug;                       // Debug mode | 调试模式

    // -------------------------------------------------------------------------
    // State Variables | 状态变量
    // -------------------------------------------------------------------------
    bool _apModeActive;                // AP mode is active | AP 模式激活
    bool _wifiConnected;               // WiFi connected | WiFi 已连接
    int _networkCount;                 // Scanned network count | 扫描到的网络数量

    // -------------------------------------------------------------------------
    // Reset Button Variables | 重置按钮变量
    // -------------------------------------------------------------------------
    int _resetButtonPin;               // Reset button GPIO pin | 重置按钮 GPIO 引脚
    bool _resetButtonActiveLow;        // Button is active LOW | 按钮低电平有效
    bool _resetButtonEnabled;          // Reset button feature enabled | 重置按钮功能已启用
    uint32_t _resetButtonPressTime;    // Time when button was pressed | 按钮按下的时间
    bool _resetButtonLastState;        // Last button state | 上一次按钮状态

    // -------------------------------------------------------------------------
    // Network Services | 网络服务
    // -------------------------------------------------------------------------
    WebServer* _webServer;             // HTTP server | HTTP 服务器
    DNSServer* _dnsServer;             // DNS server for captive portal | 强制门户 DNS 服务器

    // -------------------------------------------------------------------------
    // Callbacks | 回调
    // -------------------------------------------------------------------------
    WiFiConnectedCallback _onConnectedCallback;
    WiFiFailedCallback _onFailedCallback;
    APStartedCallback _onAPStartedCallback;

    // -------------------------------------------------------------------------
    // Internal Methods | 内部方法
    // -------------------------------------------------------------------------

    // Try to connect to WiFi | 尝试连接 WiFi
    bool _connectWiFi(const String& ssid, const String& password);

    // Setup web server routes | 设置 Web 服务器路由
    void _setupWebServer();

    // HTTP handlers | HTTP 处理器
    void _handleRoot();
    void _handleScan();
    void _handleConnect();
    void _handleStatus();
    void _handleReset();
    void _handleNotFound();

    // Generate HTML pages | 生成 HTML 页面
    String _generateMainPage();
    void _sendMainPageChunked();  // Send HTML in chunks to avoid memory issues | 分块发送 HTML 避免内存问题
    String _generateNetworkListJSON();
    String _generateStatusJSON();

    // Helper functions | 辅助函数
    String _getEncryptionTypeName(uint8_t encType);
    int _getSignalStrength(int32_t rssi);

    // Reset button handling | 重置按钮处理
    void _handleResetButton();

    // Debug log | 调试日志
    void _log(const String& message);
};

#endif // SEEED_WIFI_PROVISIONING_H

