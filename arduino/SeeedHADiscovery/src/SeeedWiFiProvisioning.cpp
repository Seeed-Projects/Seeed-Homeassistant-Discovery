/**
 * ============================================================================
 * Seeed WiFi Provisioning - Implementation File
 * Seeed WiFi Provisioning - 实现文件
 * ============================================================================
 *
 * This file contains the implementation of SeeedWiFiProvisioning class.
 * 这个文件包含 SeeedWiFiProvisioning 类的实现。
 *
 * @author limengdu
 */

#include "SeeedWiFiProvisioning.h"

// =============================================================================
// Constructor & Destructor | 构造函数和析构函数
// =============================================================================

SeeedWiFiProvisioning::SeeedWiFiProvisioning() :
    _apSSID(SEEED_WIFI_PROV_DEFAULT_AP_SSID),
    _apPassword(SEEED_WIFI_PROV_DEFAULT_AP_PASSWORD),
    _connectTimeout(SEEED_WIFI_PROV_CONNECT_TIMEOUT),
    _debug(false),
    _apModeActive(false),
    _wifiConnected(false),
    _networkCount(0),
    _resetButtonPin(-1),
    _resetButtonActiveLow(true),
    _resetButtonEnabled(false),
    _resetButtonPressTime(0),
    _resetButtonLastState(true),
    _webServer(nullptr),
    _dnsServer(nullptr),
    _onConnectedCallback(nullptr),
    _onFailedCallback(nullptr),
    _onAPStartedCallback(nullptr)
{
    // Constructor initialization complete | 构造函数初始化完成
}

SeeedWiFiProvisioning::~SeeedWiFiProvisioning() {
    stopAPMode();
}

// =============================================================================
// Configuration Methods | 配置方法
// =============================================================================

void SeeedWiFiProvisioning::setAPSSID(const String& ssid) {
    _apSSID = ssid;
}

void SeeedWiFiProvisioning::setAPPassword(const String& password) {
    _apPassword = password;
}

void SeeedWiFiProvisioning::setConnectTimeout(uint32_t timeout) {
    _connectTimeout = timeout;
}

void SeeedWiFiProvisioning::enableDebug(bool enable) {
    _debug = enable;
}

// =============================================================================
// Callback Registration | 回调注册
// =============================================================================

void SeeedWiFiProvisioning::onWiFiConnected(WiFiConnectedCallback callback) {
    _onConnectedCallback = callback;
}

void SeeedWiFiProvisioning::onWiFiFailed(WiFiFailedCallback callback) {
    _onFailedCallback = callback;
}

void SeeedWiFiProvisioning::onAPStarted(APStartedCallback callback) {
    _onAPStartedCallback = callback;
}

// =============================================================================
// Connection Methods | 连接方法
// =============================================================================

bool SeeedWiFiProvisioning::begin() {
    _log("====================================");
    _log("Seeed WiFi Provisioning starting...");
    _log("====================================");

    // Check for saved credentials | 检查是否有保存的凭据
    if (hasCredentials()) {
        String savedSSID = getSavedSSID();
        _log("Found saved credentials for: " + savedSSID);

        // Load password | 加载密码
        Preferences prefs;
        prefs.begin(SEEED_WIFI_PROV_PREFS_NAMESPACE, true);
        String savedPassword = prefs.getString(SEEED_WIFI_PROV_PREFS_PASS_KEY, "");
        prefs.end();

        // Try to connect | 尝试连接
        if (_connectWiFi(savedSSID, savedPassword)) {
            _wifiConnected = true;
            _log("WiFi connected successfully!");
            _log("IP Address: " + WiFi.localIP().toString());

            if (_onConnectedCallback) {
                _onConnectedCallback();
            }
            return true;
        }

        _log("Failed to connect to saved network");
    } else {
        _log("No saved credentials found");
    }

    // Connection failed, start AP mode | 连接失败，启动 AP 模式
    if (_onFailedCallback) {
        _onFailedCallback();
    }

    return startAPMode();
}

bool SeeedWiFiProvisioning::begin(const char* ssid, const char* password, bool saveCredentials) {
    _log("====================================");
    _log("Seeed WiFi Provisioning starting...");
    _log("====================================");
    _log("Connecting to: " + String(ssid));

    // Try to connect with provided credentials | 使用提供的凭据尝试连接
    if (_connectWiFi(String(ssid), String(password))) {
        _wifiConnected = true;
        _log("WiFi connected successfully!");
        _log("IP Address: " + WiFi.localIP().toString());

        // Save credentials if requested | 如果需要，保存凭据
        if (saveCredentials) {
            this->saveCredentials(String(ssid), String(password));
        }

        if (_onConnectedCallback) {
            _onConnectedCallback();
        }
        return true;
    }

    _log("Failed to connect to " + String(ssid));

    // Connection failed, start AP mode | 连接失败，启动 AP 模式
    if (_onFailedCallback) {
        _onFailedCallback();
    }

    return startAPMode();
}

bool SeeedWiFiProvisioning::_connectWiFi(const String& ssid, const String& password) {
    _log("Attempting to connect to: " + ssid);

    // Set WiFi mode to station | 设置 WiFi 模式为 Station
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection with timeout | 等待连接，带超时
    uint32_t startTime = millis();
    int dots = 0;

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > _connectTimeout) {
            _log("\nConnection timeout");
            WiFi.disconnect(true);
            return false;
        }

        delay(500);
        if (_debug) {
            Serial.print(".");
            dots++;
            if (dots >= 60) {
                dots = 0;
                Serial.println();
            }
        }
    }

    if (_debug) {
        Serial.println();
    }

    return true;
}

bool SeeedWiFiProvisioning::startAPMode() {
    if (_apModeActive) {
        _log("AP mode is already active");
        return true;
    }

    _log("Starting AP mode...");
    _log("AP SSID: " + _apSSID);

    // Disconnect from any network | 断开任何网络连接
    WiFi.disconnect(true);
    delay(100);

    // Set WiFi mode to AP | 设置 WiFi 模式为 AP
    WiFi.mode(WIFI_AP);

    // Start AP | 启动 AP
    bool apStarted = false;
    if (_apPassword.length() > 0) {
        apStarted = WiFi.softAP(_apSSID.c_str(), _apPassword.c_str(),
                                SEEED_WIFI_PROV_AP_CHANNEL,
                                false,  // Not hidden | 不隐藏
                                SEEED_WIFI_PROV_AP_MAX_CONNECTIONS);
    } else {
        apStarted = WiFi.softAP(_apSSID.c_str());
    }

    if (!apStarted) {
        _log("Failed to start AP mode!");
        return false;
    }

    // Configure AP IP | 配置 AP IP
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);

    _log("AP IP address: " + WiFi.softAPIP().toString());

    // Start DNS server for captive portal | 启动 DNS 服务器用于强制门户
    _dnsServer = new DNSServer();
    _dnsServer->start(SEEED_WIFI_PROV_DNS_PORT, "*", apIP);
    _log("DNS server started");

    // Start web server | 启动 Web 服务器
    _webServer = new WebServer(SEEED_WIFI_PROV_HTTP_PORT);
    _setupWebServer();
    _webServer->begin();
    _log("Web server started on port " + String(SEEED_WIFI_PROV_HTTP_PORT));

    // Scan networks initially | 初始扫描网络
    scanNetworks();

    _apModeActive = true;

    _log("====================================");
    _log("AP Mode Active!");
    _log("Connect to WiFi: " + _apSSID);
    _log("Open browser: http://" + WiFi.softAPIP().toString());
    _log("====================================");

    if (_onAPStartedCallback) {
        _onAPStartedCallback();
    }

    return false;  // Return false to indicate WiFi not connected | 返回 false 表示 WiFi 未连接
}

void SeeedWiFiProvisioning::stopAPMode() {
    if (!_apModeActive) {
        return;
    }

    _log("Stopping AP mode...");

    // Stop DNS server | 停止 DNS 服务器
    if (_dnsServer) {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }

    // Stop web server | 停止 Web 服务器
    if (_webServer) {
        _webServer->stop();
        delete _webServer;
        _webServer = nullptr;
    }

    // Stop AP | 停止 AP
    WiFi.softAPdisconnect(true);

    _apModeActive = false;
    _log("AP mode stopped");
}

// =============================================================================
// Runtime Methods | 运行时方法
// =============================================================================

void SeeedWiFiProvisioning::handle() {
    // Handle reset button (check for long press)
    // 处理重置按钮（检测长按）
    if (_resetButtonEnabled) {
        _handleResetButton();
    }

    if (_apModeActive) {
        // Handle DNS requests for captive portal | 处理 DNS 请求用于强制门户
        if (_dnsServer) {
            _dnsServer->processNextRequest();
        }

        // Handle HTTP requests | 处理 HTTP 请求
        if (_webServer) {
            _webServer->handleClient();
        }
    }
}

// =============================================================================
// Status Queries | 状态查询
// =============================================================================

bool SeeedWiFiProvisioning::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool SeeedWiFiProvisioning::isAPModeActive() const {
    return _apModeActive;
}

bool SeeedWiFiProvisioning::hasCredentials() const {
    Preferences prefs;
    prefs.begin(SEEED_WIFI_PROV_PREFS_NAMESPACE, true);
    bool configured = prefs.getBool(SEEED_WIFI_PROV_PREFS_CONFIG_KEY, false);
    prefs.end();
    return configured;
}

String SeeedWiFiProvisioning::getSavedSSID() const {
    Preferences prefs;
    prefs.begin(SEEED_WIFI_PROV_PREFS_NAMESPACE, true);
    String ssid = prefs.getString(SEEED_WIFI_PROV_PREFS_SSID_KEY, "");
    prefs.end();
    return ssid;
}

IPAddress SeeedWiFiProvisioning::getLocalIP() const {
    return WiFi.localIP();
}

IPAddress SeeedWiFiProvisioning::getAPIP() const {
    return WiFi.softAPIP();
}

// =============================================================================
// Credential Management | 凭据管理
// =============================================================================

void SeeedWiFiProvisioning::clearCredentials() {
    _log("Clearing saved credentials...");
    Preferences prefs;
    prefs.begin(SEEED_WIFI_PROV_PREFS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    _log("Credentials cleared");
}

void SeeedWiFiProvisioning::saveCredentials(const String& ssid, const String& password) {
    _log("Saving credentials for: " + ssid);
    Preferences prefs;
    prefs.begin(SEEED_WIFI_PROV_PREFS_NAMESPACE, false);
    prefs.putString(SEEED_WIFI_PROV_PREFS_SSID_KEY, ssid);
    prefs.putString(SEEED_WIFI_PROV_PREFS_PASS_KEY, password);
    prefs.putBool(SEEED_WIFI_PROV_PREFS_CONFIG_KEY, true);
    prefs.end();
    _log("Credentials saved");
}

// =============================================================================
// Reset Button | 重置按钮
// =============================================================================

void SeeedWiFiProvisioning::enableResetButton(int pin, bool activeLow) {
    _resetButtonPin = pin;
    _resetButtonActiveLow = activeLow;
    _resetButtonEnabled = true;
    _resetButtonPressTime = 0;
    
    // Configure pin with appropriate pull resistor
    // 配置引脚并设置合适的上拉/下拉电阻
    if (activeLow) {
        pinMode(pin, INPUT_PULLUP);
        _resetButtonLastState = HIGH;  // Not pressed | 未按下
    } else {
        pinMode(pin, INPUT_PULLDOWN);
        _resetButtonLastState = LOW;   // Not pressed | 未按下
    }
    
    _log("Reset button enabled on GPIO" + String(pin) + 
         (activeLow ? " (active LOW)" : " (active HIGH)"));
    _log("Long press " + String(SEEED_WIFI_PROV_RESET_HOLD_TIME / 1000) + 
         "s to clear credentials and start AP mode");
}

void SeeedWiFiProvisioning::disableResetButton() {
    _resetButtonEnabled = false;
    _resetButtonPin = -1;
    _log("Reset button disabled");
}

void SeeedWiFiProvisioning::_handleResetButton() {
    if (_resetButtonPin < 0) return;
    
    bool currentState = digitalRead(_resetButtonPin);
    bool isPressed = (_resetButtonActiveLow) ? (currentState == LOW) : (currentState == HIGH);
    bool wasPressed = (_resetButtonActiveLow) ? (_resetButtonLastState == LOW) : (_resetButtonLastState == HIGH);
    
    // Button just pressed | 按钮刚被按下
    if (isPressed && !wasPressed) {
        _resetButtonPressTime = millis();
        _log("Reset button pressed - hold for " + 
             String(SEEED_WIFI_PROV_RESET_HOLD_TIME / 1000) + "s to reset WiFi");
    }
    
    // Button is being held | 按钮被按住
    if (isPressed && _resetButtonPressTime > 0) {
        uint32_t holdTime = millis() - _resetButtonPressTime;
        
        // Check if held long enough | 检查是否按住足够长时间
        if (holdTime >= SEEED_WIFI_PROV_RESET_HOLD_TIME) {
            _log("=========================================");
            _log("Reset button held for 6 seconds!");
            _log("Clearing credentials and starting AP mode...");
            _log("=========================================");
            
            // Clear credentials | 清除凭据
            clearCredentials();
            
            // Disconnect WiFi if connected | 如果已连接则断开 WiFi
            if (WiFi.status() == WL_CONNECTED) {
                WiFi.disconnect(true);
                _wifiConnected = false;
            }
            
            // Start AP mode | 启动 AP 模式
            startAPMode();
            
            // Reset press time to prevent re-triggering | 重置按压时间防止重复触发
            _resetButtonPressTime = 0;
        }
    }
    
    // Button released | 按钮被释放
    if (!isPressed && wasPressed) {
        if (_resetButtonPressTime > 0) {
            uint32_t holdTime = millis() - _resetButtonPressTime;
            if (holdTime < SEEED_WIFI_PROV_RESET_HOLD_TIME) {
                _log("Reset button released early (" + String(holdTime) + "ms)");
            }
        }
        _resetButtonPressTime = 0;
    }
    
    _resetButtonLastState = currentState;
}

// =============================================================================
// Network Scanning | 网络扫描
// =============================================================================

int SeeedWiFiProvisioning::scanNetworks() {
    _log("Scanning for WiFi networks...");

    // Scan for networks (blocking) | 扫描网络（阻塞）
    _networkCount = WiFi.scanNetworks();

    _log("Found " + String(_networkCount) + " networks");

    return _networkCount;
}

int SeeedWiFiProvisioning::getNetworkCount() const {
    return _networkCount;
}

String SeeedWiFiProvisioning::getNetworkSSID(int index) const {
    if (index < 0 || index >= _networkCount) {
        return "";
    }
    return WiFi.SSID(index);
}

int32_t SeeedWiFiProvisioning::getNetworkRSSI(int index) const {
    if (index < 0 || index >= _networkCount) {
        return 0;
    }
    return WiFi.RSSI(index);
}

uint8_t SeeedWiFiProvisioning::getNetworkEncryption(int index) const {
    if (index < 0 || index >= _networkCount) {
        return 0;
    }
    return WiFi.encryptionType(index);
}

// =============================================================================
// Web Server Setup | Web 服务器设置
// =============================================================================

void SeeedWiFiProvisioning::_setupWebServer() {
    // Main page | 主页
    _webServer->on("/", HTTP_GET, [this]() {
        _handleRoot();
    });

    // Scan networks API | 扫描网络 API
    _webServer->on("/scan", HTTP_GET, [this]() {
        _handleScan();
    });

    // Connect API | 连接 API
    _webServer->on("/connect", HTTP_POST, [this]() {
        _handleConnect();
    });

    // Status API | 状态 API
    _webServer->on("/status", HTTP_GET, [this]() {
        _handleStatus();
    });

    // Reset API | 重置 API
    _webServer->on("/reset", HTTP_POST, [this]() {
        _handleReset();
    });

    // Captive portal detection | 强制门户检测
    _webServer->on("/generate_204", HTTP_GET, [this]() {
        _handleRoot();
    });
    _webServer->on("/fwlink", HTTP_GET, [this]() {
        _handleRoot();
    });
    _webServer->on("/hotspot-detect.html", HTTP_GET, [this]() {
        _handleRoot();
    });
    _webServer->on("/canonical.html", HTTP_GET, [this]() {
        _handleRoot();
    });
    _webServer->on("/success.txt", HTTP_GET, [this]() {
        _webServer->send(200, "text/plain", "success");
    });
    _webServer->on("/ncsi.txt", HTTP_GET, [this]() {
        _webServer->send(200, "text/plain", "Microsoft NCSI");
    });

    // Handle all other requests | 处理所有其他请求
    _webServer->onNotFound([this]() {
        _handleNotFound();
    });
}

// =============================================================================
// HTTP Handlers | HTTP 处理器
// =============================================================================

void SeeedWiFiProvisioning::_handleRoot() {
    _log("HTTP Request: / (root page)");
    
    // Use chunked transfer to avoid large memory allocation
    // 使用分块传输避免大内存分配
    _webServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
    _webServer->send(200, "text/html; charset=utf-8", "");
    
    // Send HTML in chunks | 分块发送 HTML
    _sendMainPageChunked();
    
    // End the response | 结束响应
    _webServer->sendContent("");
}

void SeeedWiFiProvisioning::_handleScan() {
    _log("Scan request received");
    scanNetworks();
    String json = _generateNetworkListJSON();
    _webServer->send(200, "application/json", json);
}

void SeeedWiFiProvisioning::_handleConnect() {
    _log("Connect request received");

    if (!_webServer->hasArg("ssid")) {
        _webServer->send(400, "application/json", "{\"success\":false,\"error\":\"Missing SSID\"}");
        return;
    }

    String ssid = _webServer->arg("ssid");
    String password = _webServer->hasArg("password") ? _webServer->arg("password") : "";

    _log("Connecting to: " + ssid);

    // Send response before attempting connection | 在尝试连接之前发送响应
    _webServer->send(200, "application/json", "{\"success\":true,\"message\":\"Connecting...\"}");

    // Small delay to ensure response is sent | 小延迟确保响应发送
    delay(500);

    // Stop AP services temporarily | 暂时停止 AP 服务
    if (_dnsServer) {
        _dnsServer->stop();
    }

    // Try to connect | 尝试连接
    WiFi.mode(WIFI_AP_STA);  // Both AP and STA | AP 和 STA 同时
    
    if (_connectWiFi(ssid, password)) {
        // Success! Save credentials and restart | 成功！保存凭据并重启
        saveCredentials(ssid, password);
        _wifiConnected = true;

        _log("Connected successfully! Restarting...");

        if (_onConnectedCallback) {
            _onConnectedCallback();
        }

        // Restart device | 重启设备
        delay(1000);
        ESP.restart();
    } else {
        // Failed, restore AP mode | 失败，恢复 AP 模式
        _log("Connection failed, restoring AP mode");

        WiFi.mode(WIFI_AP);
        if (_dnsServer) {
            _dnsServer->start(SEEED_WIFI_PROV_DNS_PORT, "*", WiFi.softAPIP());
        }

        if (_onFailedCallback) {
            _onFailedCallback();
        }
    }
}

void SeeedWiFiProvisioning::_handleStatus() {
    String json = _generateStatusJSON();
    _webServer->send(200, "application/json", json);
}

void SeeedWiFiProvisioning::_handleReset() {
    _log("Reset request received");
    clearCredentials();
    _webServer->send(200, "application/json", "{\"success\":true,\"message\":\"Credentials cleared\"}");
}

void SeeedWiFiProvisioning::_handleNotFound() {
    // Redirect all unknown requests to main page (captive portal behavior)
    // 将所有未知请求重定向到主页（强制门户行为）
    _webServer->sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    _webServer->send(302, "text/plain", "");
}

// =============================================================================
// HTML/JSON Generation | HTML/JSON 生成
// =============================================================================

String SeeedWiFiProvisioning::_generateMainPage() {
    String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>Seeed WiFi Setup</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&display=swap');
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        :root {
            --bg-primary: #0a0a0f;
            --bg-secondary: #12121a;
            --bg-card: #1a1a24;
            --accent: #00ff9d;
            --accent-dim: #00cc7d;
            --accent-glow: rgba(0, 255, 157, 0.3);
            --text-primary: #e8e8e8;
            --text-secondary: #888;
            --border: #2a2a35;
            --danger: #ff4757;
            --warning: #ffa502;
        }
        
        body {
            font-family: 'Space Mono', monospace;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            padding: 20px;
            background-image: 
                radial-gradient(circle at 20% 80%, rgba(0, 255, 157, 0.05) 0%, transparent 50%),
                radial-gradient(circle at 80% 20%, rgba(0, 200, 255, 0.05) 0%, transparent 50%);
        }
        
        .container {
            max-width: 420px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            margin-bottom: 30px;
            padding: 20px;
        }
        
        .logo {
            font-size: 2.2em;
            font-weight: 700;
            color: var(--accent);
            text-shadow: 0 0 30px var(--accent-glow);
            letter-spacing: 3px;
            margin-bottom: 8px;
        }
        
        .subtitle {
            font-size: 0.85em;
            color: var(--text-secondary);
            letter-spacing: 2px;
        }
        
        .card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 24px;
            margin-bottom: 20px;
            position: relative;
            overflow: hidden;
        }
        
        .card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 2px;
            background: linear-gradient(90deg, transparent, var(--accent), transparent);
        }
        
        .section-title {
            font-size: 0.75em;
            color: var(--accent);
            letter-spacing: 3px;
            margin-bottom: 16px;
            text-transform: uppercase;
        }
        
        .network-list {
            max-height: 280px;
            overflow-y: auto;
            margin: -8px;
            padding: 8px;
        }
        
        .network-list::-webkit-scrollbar {
            width: 4px;
        }
        
        .network-list::-webkit-scrollbar-track {
            background: var(--bg-secondary);
        }
        
        .network-list::-webkit-scrollbar-thumb {
            background: var(--accent-dim);
            border-radius: 2px;
        }
        
        .network-item {
            display: flex;
            align-items: center;
            padding: 14px 16px;
            margin-bottom: 8px;
            background: var(--bg-secondary);
            border: 1px solid transparent;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        
        .network-item:hover {
            border-color: var(--accent);
            background: rgba(0, 255, 157, 0.05);
        }
        
        .network-item.selected {
            border-color: var(--accent);
            background: rgba(0, 255, 157, 0.1);
            box-shadow: 0 0 20px var(--accent-glow);
        }
        
        .network-icon {
            width: 24px;
            height: 24px;
            margin-right: 14px;
            opacity: 0.8;
        }
        
        .signal-bars {
            display: flex;
            align-items: flex-end;
            gap: 2px;
            height: 16px;
        }
        
        .signal-bar {
            width: 3px;
            background: var(--border);
            border-radius: 1px;
        }
        
        .signal-bar.active {
            background: var(--accent);
        }
        
        .signal-bar:nth-child(1) { height: 4px; }
        .signal-bar:nth-child(2) { height: 8px; }
        .signal-bar:nth-child(3) { height: 12px; }
        .signal-bar:nth-child(4) { height: 16px; }
        
        .network-info {
            flex: 1;
        }
        
        .network-name {
            font-size: 0.95em;
            margin-bottom: 2px;
        }
        
        .network-meta {
            font-size: 0.7em;
            color: var(--text-secondary);
        }
        
        .lock-icon {
            margin-left: 10px;
            opacity: 0.6;
        }
        
        .form-group {
            margin-bottom: 20px;
        }
        
        .form-label {
            display: block;
            font-size: 0.75em;
            color: var(--text-secondary);
            letter-spacing: 1px;
            margin-bottom: 8px;
            text-transform: uppercase;
        }
        
        .form-input {
            width: 100%;
            padding: 14px 16px;
            background: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 8px;
            color: var(--text-primary);
            font-family: 'Space Mono', monospace;
            font-size: 0.95em;
            transition: all 0.2s ease;
        }
        
        .form-input:focus {
            outline: none;
            border-color: var(--accent);
            box-shadow: 0 0 20px var(--accent-glow);
        }
        
        .form-input::placeholder {
            color: var(--text-secondary);
        }
        
        .btn {
            width: 100%;
            padding: 16px 24px;
            font-family: 'Space Mono', monospace;
            font-size: 0.85em;
            font-weight: 700;
            letter-spacing: 2px;
            text-transform: uppercase;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        
        .btn-primary {
            background: var(--accent);
            color: var(--bg-primary);
        }
        
        .btn-primary:hover:not(:disabled) {
            background: var(--accent-dim);
            box-shadow: 0 0 30px var(--accent-glow);
        }
        
        .btn-primary:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        
        .btn-secondary {
            background: transparent;
            color: var(--text-secondary);
            border: 1px solid var(--border);
        }
        
        .btn-secondary:hover {
            border-color: var(--accent);
            color: var(--accent);
        }
        
        .btn-danger {
            background: transparent;
            color: var(--danger);
            border: 1px solid var(--danger);
        }
        
        .btn-danger:hover {
            background: rgba(255, 71, 87, 0.1);
        }
        
        .status-message {
            padding: 14px 16px;
            border-radius: 8px;
            font-size: 0.85em;
            margin-bottom: 20px;
            display: none;
        }
        
        .status-message.show {
            display: block;
        }
        
        .status-message.success {
            background: rgba(0, 255, 157, 0.1);
            border: 1px solid var(--accent);
            color: var(--accent);
        }
        
        .status-message.error {
            background: rgba(255, 71, 87, 0.1);
            border: 1px solid var(--danger);
            color: var(--danger);
        }
        
        .status-message.info {
            background: rgba(255, 165, 2, 0.1);
            border: 1px solid var(--warning);
            color: var(--warning);
        }
        
        .loading {
            display: inline-block;
            width: 16px;
            height: 16px;
            border: 2px solid var(--border);
            border-top-color: var(--accent);
            border-radius: 50%;
            animation: spin 1s linear infinite;
            margin-right: 10px;
            vertical-align: middle;
        }
        
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        
        .pull-hint {
            text-align: center;
            font-size: 0.7em;
            color: var(--text-secondary);
            padding: 10px;
            margin-top: -8px;
        }
        
        .footer {
            text-align: center;
            padding: 20px;
            font-size: 0.7em;
            color: var(--text-secondary);
        }
        
        .footer a {
            color: var(--accent);
            text-decoration: none;
        }
        
        .hidden {
            display: none !important;
        }
        
        @media (max-width: 480px) {
            body {
                padding: 15px;
            }
            
            .card {
                padding: 18px;
            }
            
            .logo {
                font-size: 1.8em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="logo">SEEED</div>
            <div class="subtitle">WiFi Configuration</div>
        </div>

        <div id="statusMessage" class="status-message"></div>

        <div class="card">
            <div class="section-title">Available Networks</div>
            <div id="networkList" class="network-list">
                <div style="text-align: center; padding: 20px; color: var(--text-secondary);">
                    <span class="loading"></span> Scanning...
                </div>
            </div>
            <div class="pull-hint">Click "Refresh" to scan again</div>
        </div>

        <div class="card" id="connectForm">
            <div class="section-title">Connect</div>
            
            <div class="form-group">
                <label class="form-label">Network Name (SSID)</label>
                <input type="text" id="ssidInput" class="form-input" placeholder="Select a network above" readonly>
            </div>
            
            <div class="form-group" id="passwordGroup">
                <label class="form-label">Password</label>
                <input type="password" id="passwordInput" class="form-input" placeholder="Enter password">
            </div>
            
            <button id="connectBtn" class="btn btn-primary" disabled>Connect</button>
        </div>

        <div style="display: flex; gap: 10px; margin-bottom: 20px;">
            <button id="refreshBtn" class="btn btn-secondary" style="flex: 1;">Refresh</button>
            <button id="resetBtn" class="btn btn-danger" style="flex: 1;">Reset</button>
        </div>

        <div class="footer">
            Powered by <a href="https://www.seeedstudio.com">Seeed Studio</a>
        </div>
    </div>

    <script>
        let selectedSSID = '';
        let selectedSecure = false;

        // Initialize | 初始化
        document.addEventListener('DOMContentLoaded', function() {
            scanNetworks();
            setupEventListeners();
        });

        function setupEventListeners() {
            document.getElementById('connectBtn').addEventListener('click', connect);
            document.getElementById('refreshBtn').addEventListener('click', scanNetworks);
            document.getElementById('resetBtn').addEventListener('click', resetCredentials);
            document.getElementById('passwordInput').addEventListener('keypress', function(e) {
                if (e.key === 'Enter') connect();
            });
        }

        function showStatus(message, type) {
            const el = document.getElementById('statusMessage');
            el.textContent = message;
            el.className = 'status-message show ' + type;
        }

        function hideStatus() {
            document.getElementById('statusMessage').className = 'status-message';
        }

        function scanNetworks() {
            const list = document.getElementById('networkList');
            list.innerHTML = '<div style="text-align: center; padding: 20px; color: var(--text-secondary);"><span class="loading"></span> Scanning...</div>';
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    renderNetworks(data.networks);
                })
                .catch(error => {
                    list.innerHTML = '<div style="text-align: center; padding: 20px; color: var(--danger);">Scan failed. Please try again.</div>';
                    showStatus('Network scan failed', 'error');
                });
        }

        function renderNetworks(networks) {
            const list = document.getElementById('networkList');
            
            if (!networks || networks.length === 0) {
                list.innerHTML = '<div style="text-align: center; padding: 20px; color: var(--text-secondary);">No networks found</div>';
                return;
            }

            let html = '';
            networks.forEach((network, index) => {
                const signalBars = getSignalBars(network.rssi);
                const isSecure = network.secure;
                const isSelected = network.ssid === selectedSSID;
                
                html += `
                    <div class="network-item ${isSelected ? 'selected' : ''}" 
                         onclick="selectNetwork('${escapeHtml(network.ssid)}', ${isSecure})">
                        <div class="signal-bars">
                            ${signalBars}
                        </div>
                        <div class="network-info">
                            <div class="network-name">${escapeHtml(network.ssid)}</div>
                            <div class="network-meta">${network.rssi} dBm · ${network.encryption}</div>
                        </div>
                        ${isSecure ? '<svg class="lock-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 10 0v4"></path></svg>' : ''}
                    </div>
                `;
            });
            
            list.innerHTML = html;
        }

        function getSignalBars(rssi) {
            let strength = 0;
            if (rssi >= -50) strength = 4;
            else if (rssi >= -60) strength = 3;
            else if (rssi >= -70) strength = 2;
            else strength = 1;
            
            let bars = '';
            for (let i = 1; i <= 4; i++) {
                bars += `<div class="signal-bar ${i <= strength ? 'active' : ''}"></div>`;
            }
            return bars;
        }

        function selectNetwork(ssid, secure) {
            selectedSSID = ssid;
            selectedSecure = secure;
            
            document.getElementById('ssidInput').value = ssid;
            document.getElementById('connectBtn').disabled = false;
            
            const passwordGroup = document.getElementById('passwordGroup');
            if (secure) {
                passwordGroup.classList.remove('hidden');
                document.getElementById('passwordInput').focus();
            } else {
                passwordGroup.classList.add('hidden');
                document.getElementById('passwordInput').value = '';
            }

            // Update selection visual | 更新选择状态视觉效果
            document.querySelectorAll('.network-item').forEach(item => {
                item.classList.remove('selected');
            });
            event.currentTarget.classList.add('selected');
        }

        function connect() {
            if (!selectedSSID) {
                showStatus('Please select a network', 'error');
                return;
            }

            const password = document.getElementById('passwordInput').value;
            
            if (selectedSecure && !password) {
                showStatus('Password is required for this network', 'error');
                return;
            }

            const connectBtn = document.getElementById('connectBtn');
            connectBtn.disabled = true;
            connectBtn.innerHTML = '<span class="loading"></span> Connecting...';

            const formData = new FormData();
            formData.append('ssid', selectedSSID);
            formData.append('password', password);

            fetch('/connect', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                // Show completion message immediately
                // The device will disconnect from AP and try to connect to WiFi
                showProvisioningComplete();
            })
            .catch(error => {
                // This is expected - device disconnects from AP to connect to WiFi
                showProvisioningComplete();
            });
        }

        function showProvisioningComplete() {
            // Hide the forms and show completion message
            document.getElementById('connectForm').style.display = 'none';
            document.querySelectorAll('.card')[0].style.display = 'none';
            document.querySelector('[style*="display: flex"]').style.display = 'none';
            
            // Create and show completion message
            const container = document.querySelector('.container');
            const completionDiv = document.createElement('div');
            completionDiv.className = 'card';
            completionDiv.innerHTML = `
                <div class="section-title" style="color: var(--accent);">Configuration Complete!</div>
                <p style="margin-bottom: 16px; line-height: 1.6;">
                    WiFi credentials have been saved. The device is now attempting to connect to your network.
                </p>
                <p style="margin-bottom: 16px; line-height: 1.6;">
                    <strong>This hotspot will disconnect.</strong> Please check:
                </p>
                <ul style="margin-bottom: 20px; padding-left: 20px; line-height: 1.8; color: var(--text-secondary);">
                    <li>Device LED indicators for connection status</li>
                    <li>Serial monitor for detailed logs</li>
                    <li>Your Home Assistant for the new device</li>
                </ul>
                <p style="font-size: 0.85em; color: var(--text-secondary);">
                    If connection fails, the device will restart in AP mode. Connect to the hotspot again to retry.
                </p>
            `;
            container.insertBefore(completionDiv, document.querySelector('.footer'));
            
            // Update header
            document.querySelector('.subtitle').textContent = 'Setup Complete';
        }

        function resetCredentials() {
            if (!confirm('Are you sure you want to clear saved WiFi credentials?')) {
                return;
            }

            fetch('/reset', { method: 'POST' })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showStatus('Credentials cleared successfully', 'success');
                    }
                })
                .catch(error => {
                    showStatus('Failed to clear credentials', 'error');
                });
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
    </script>
</body>
</html>)rawliteral";

    return html;
}

String SeeedWiFiProvisioning::_generateNetworkListJSON() {
    String json = "{\"networks\":[";

    for (int i = 0; i < _networkCount; i++) {
        if (i > 0) json += ",";

        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        uint8_t encType = WiFi.encryptionType(i);
        bool secure = (encType != WIFI_AUTH_OPEN);

        // Escape SSID for JSON | 为 JSON 转义 SSID
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");

        json += "{";
        json += "\"ssid\":\"" + ssid + "\",";
        json += "\"rssi\":" + String(rssi) + ",";
        json += "\"secure\":" + String(secure ? "true" : "false") + ",";
        json += "\"encryption\":\"" + _getEncryptionTypeName(encType) + "\"";
        json += "}";
    }

    json += "]}";
    return json;
}

String SeeedWiFiProvisioning::_generateStatusJSON() {
    String json = "{";
    json += "\"wifi_connected\":" + String(isWiFiConnected() ? "true" : "false") + ",";
    json += "\"ap_active\":" + String(_apModeActive ? "true" : "false") + ",";
    json += "\"has_credentials\":" + String(hasCredentials() ? "true" : "false") + ",";
    json += "\"saved_ssid\":\"" + getSavedSSID() + "\",";
    json += "\"ip\":\"" + (isWiFiConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
    json += "}";
    return json;
}

// =============================================================================
// Chunked HTML Sending | 分块 HTML 发送
// =============================================================================

void SeeedWiFiProvisioning::_sendMainPageChunked() {
    // Send HTML head and CSS in smaller chunks to avoid memory allocation issues
    // 分块发送 HTML 头部和 CSS 以避免内存分配问题
    
    // Chunk 1: HTML head start
    _webServer->sendContent(F("<!DOCTYPE html><html lang=\"en\"><head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,user-scalable=no\">"
        "<title>Seeed WiFi Setup</title><style>"));
    
    // Chunk 2: CSS variables and base styles
    _webServer->sendContent(F(
        "*{margin:0;padding:0;box-sizing:border-box}"
        ":root{--bg:#0a0a0f;--bg2:#1a1a24;--accent:#00ff9d;--text:#e8e8e8;--text2:#888;--border:#2a2a35}"
        "body{font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;padding:20px}"
        ".container{max-width:420px;margin:0 auto}"
        ".header{text-align:center;margin-bottom:30px;padding:20px}"
        ".logo{font-size:2em;font-weight:700;color:var(--accent);margin-bottom:8px}"
        ".subtitle{font-size:0.85em;color:var(--text2)}"
    ));
    
    // Chunk 3: Card and network list styles
    _webServer->sendContent(F(
        ".card{background:var(--bg2);border:1px solid var(--border);border-radius:12px;padding:24px;margin-bottom:20px}"
        ".section-title{font-size:0.75em;color:var(--accent);letter-spacing:2px;margin-bottom:16px;text-transform:uppercase}"
        ".network-list{max-height:280px;overflow-y:auto}"
        ".network-item{display:flex;align-items:center;padding:14px 16px;margin-bottom:8px;background:var(--bg);border:1px solid transparent;border-radius:8px;cursor:pointer;transition:all 0.2s}"
        ".network-item:hover,.network-item.selected{border-color:var(--accent);background:rgba(0,255,157,0.1)}"
        ".network-info{flex:1}"
        ".network-name{font-size:0.95em;margin-bottom:2px}"
        ".network-meta{font-size:0.7em;color:var(--text2)}"
    ));
    
    // Chunk 4: Signal bars and form styles
    _webServer->sendContent(F(
        ".signal-bars{display:flex;align-items:flex-end;gap:2px;height:16px;margin-right:14px}"
        ".signal-bar{width:3px;background:var(--border);border-radius:1px}"
        ".signal-bar.active{background:var(--accent)}"
        ".signal-bar:nth-child(1){height:4px}.signal-bar:nth-child(2){height:8px}.signal-bar:nth-child(3){height:12px}.signal-bar:nth-child(4){height:16px}"
        ".form-group{margin-bottom:20px}"
        ".form-label{display:block;font-size:0.75em;color:var(--text2);margin-bottom:8px}"
        ".form-input{width:100%;padding:14px 16px;background:var(--bg);border:1px solid var(--border);border-radius:8px;color:var(--text);font-size:1em}"
        ".form-input:focus{outline:none;border-color:var(--accent)}"
    ));
    
    // Chunk 5: Button and status styles
    _webServer->sendContent(F(
        ".btn{width:100%;padding:16px;border:none;border-radius:8px;font-size:1em;font-weight:600;cursor:pointer;transition:all 0.2s}"
        ".btn-primary{background:var(--accent);color:#000}"
        ".btn-primary:hover{filter:brightness(1.1)}"
        ".btn-primary:disabled{opacity:0.5;cursor:not-allowed}"
        ".btn-secondary{background:transparent;border:1px solid var(--border);color:var(--text);margin-top:10px}"
        ".status{text-align:center;padding:12px;border-radius:8px;margin-bottom:20px;font-size:0.85em}"
        ".status.connecting{background:rgba(255,165,2,0.1);border:1px solid #ffa502;color:#ffa502}"
        ".status.success{background:rgba(0,255,157,0.1);border:1px solid var(--accent);color:var(--accent)}"
        ".status.error{background:rgba(255,71,87,0.1);border:1px solid #ff4757;color:#ff4757}"
        ".lock-icon{margin-left:10px;opacity:0.6}"
        ".spinner{display:inline-block;width:16px;height:16px;border:2px solid transparent;border-top-color:currentColor;border-radius:50%;animation:spin 1s linear infinite;margin-right:8px}"
        "@keyframes spin{to{transform:rotate(360deg)}}"
        "</style></head>"
    ));
    
    // Chunk 6: Body start and header
    _webServer->sendContent(F(
        "<body><div class=\"container\">"
        "<div class=\"header\">"
        "<div class=\"logo\">SEEED</div>"
        "<div class=\"subtitle\">WiFi Configuration</div>"
        "</div>"
        "<div id=\"status\" class=\"status\" style=\"display:none\"></div>"
        "<div class=\"card\">"
        "<div class=\"section-title\">Available Networks</div>"
        "<div id=\"networks\" class=\"network-list\"><div style=\"text-align:center;padding:20px;color:var(--text2)\">Scanning...</div></div>"
        "<button class=\"btn btn-secondary\" onclick=\"scan()\">Refresh Networks</button>"
        "</div>"
    ));
    
    // Chunk 7: Password form
    _webServer->sendContent(F(
        "<div class=\"card\" id=\"connect-form\" style=\"display:none\">"
        "<div class=\"section-title\">Connect to Network</div>"
        "<div class=\"form-group\">"
        "<label class=\"form-label\">SELECTED NETWORK</label>"
        "<div id=\"selected-network\" style=\"padding:10px;background:var(--bg);border-radius:8px\"></div>"
        "</div>"
        "<div class=\"form-group\">"
        "<label class=\"form-label\">PASSWORD</label>"
        "<input type=\"password\" id=\"password\" class=\"form-input\" placeholder=\"Enter WiFi password\">"
        "</div>"
        "<button class=\"btn btn-primary\" id=\"connect-btn\" onclick=\"connect()\">Connect</button>"
        "</div>"
    ));
    
    // Chunk 8: JavaScript start
    _webServer->sendContent(F(
        "<script>"
        "let selectedSSID='',selectedSecure=false;"
        "function scan(){"
        "document.getElementById('networks').innerHTML='<div style=\"text-align:center;padding:20px;color:var(--text2)\">Scanning...</div>';"
        "fetch('/scan').then(r=>r.json()).then(d=>{"
        "let html='';"
        "d.networks.forEach(n=>{"
        "let bars='';for(let i=1;i<=4;i++)bars+='<div class=\"signal-bar'+(i<=n.signal?' active':'')+'\"></div>';"
        "html+='<div class=\"network-item\" onclick=\"select(\\''+n.ssid+'\\','+n.secure+')\">'+"
        "'<div class=\"signal-bars\">'+bars+'</div>'+"
        "'<div class=\"network-info\"><div class=\"network-name\">'+n.ssid+'</div>'+"
        "'<div class=\"network-meta\">'+n.encryption+'</div></div>'+"
        "(n.secure?'<span class=\"lock-icon\">🔒</span>':'')+'</div>';"
        "});"
        "document.getElementById('networks').innerHTML=html||'<div style=\"text-align:center;padding:20px;color:var(--text2)\">No networks found</div>';"
        "}).catch(e=>{"
        "document.getElementById('networks').innerHTML='<div style=\"text-align:center;padding:20px;color:#ff4757\">Scan failed</div>';"
        "});"
        "}"
    ));
    
    // Chunk 9: JavaScript select and connect
    _webServer->sendContent(F(
        "function select(ssid,secure){"
        "selectedSSID=ssid;selectedSecure=secure;"
        "document.querySelectorAll('.network-item').forEach(el=>el.classList.remove('selected'));"
        "event.currentTarget.classList.add('selected');"
        "document.getElementById('selected-network').textContent=ssid;"
        "document.getElementById('connect-form').style.display='block';"
        "document.getElementById('password').style.display=secure?'block':'none';"
        "document.getElementById('password').value='';"
        "}"
        "function connect(){"
        "let btn=document.getElementById('connect-btn');"
        "btn.disabled=true;btn.innerHTML='<span class=\"spinner\"></span>Connecting...';"
        "showStatus('Connecting to '+selectedSSID+'...','connecting');"
        "let data='ssid='+encodeURIComponent(selectedSSID);"
        "if(selectedSecure)data+='&password='+encodeURIComponent(document.getElementById('password').value);"
        "fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data})"
        ".then(r=>r.json()).then(d=>{"
        "if(d.success){showStatus('Connected! Device will restart...','success');setTimeout(()=>location.reload(),5000);}"
        "else{showStatus('Connection failed: '+d.error,'error');btn.disabled=false;btn.textContent='Connect';}"
        "}).catch(e=>{showStatus('Error: '+e,'error');btn.disabled=false;btn.textContent='Connect';});"
        "}"
    ));
    
    // Chunk 10: JavaScript showStatus and init
    _webServer->sendContent(F(
        "function showStatus(msg,type){"
        "let s=document.getElementById('status');"
        "s.textContent=msg;s.className='status '+type;s.style.display='block';"
        "}"
        "scan();"
        "</script></div></body></html>"
    ));
}

// =============================================================================
// Helper Functions | 辅助函数
// =============================================================================

String SeeedWiFiProvisioning::_getEncryptionTypeName(uint8_t encType) {
    switch (encType) {
        case WIFI_AUTH_OPEN:            return "Open";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "Unknown";
    }
}

int SeeedWiFiProvisioning::_getSignalStrength(int32_t rssi) {
    if (rssi >= -50) return 4;
    if (rssi >= -60) return 3;
    if (rssi >= -70) return 2;
    return 1;
}

void SeeedWiFiProvisioning::_log(const String& message) {
    if (_debug) {
        Serial.println("[WiFiProv] " + message);
    }
}

