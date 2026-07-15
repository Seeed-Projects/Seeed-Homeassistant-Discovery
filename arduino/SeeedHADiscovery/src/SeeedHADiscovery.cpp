/**
 * ============================================================================
 * Seeed Home Assistant Discovery - Implementation File
 * Seeed Home Assistant Discovery - 实现文件
 * ============================================================================
 *
 * This file contains the implementation of SeeedHADiscovery and SeeedHASensor classes.
 * 这个文件包含 SeeedHADiscovery 和 SeeedHASensor 类的实现。
 *
 * @author limengdu
 */

#include "SeeedHADiscovery.h"
#include "SeeedWiFiProvisioning.h"

// =============================================================================
// SeeedHASensor Implementation | SeeedHASensor 实现
// =============================================================================

SeeedHASensor::SeeedHASensor(
    const String& id,
    const String& name,
    const String& deviceClass,
    const String& unit
) :
    _id(id),
    _name(name),
    _deviceClass(deviceClass),
    _unit(unit),
    _stateClass("measurement"),  // Default state class | 默认状态类别
    _icon(""),
    _value(0),
    _precision(1),               // Default 1 decimal | 默认 1 位小数
    _hasValue(false),
    _ha(nullptr)
{
    // Constructor initialization complete | 构造函数初始化完成
}

void SeeedHASensor::setValue(float value) {
    // Set sensor value | 设置传感器值
    _value = value;
    _hasValue = true;

    // Notify main class that value is updated
    // 通知主类，值已更新
    _notifyChange();
}

void SeeedHASensor::setStateClass(const String& stateClass) {
    _stateClass = stateClass;
}

void SeeedHASensor::setPrecision(int precision) {
    _precision = precision;
}

void SeeedHASensor::setIcon(const String& icon) {
    _icon = icon;
}

void SeeedHASensor::toJson(JsonObject& obj) const {
    // Convert sensor info to JSON format
    // This JSON is sent to Home Assistant
    // 将传感器信息转换为 JSON 格式
    // 这个 JSON 会发送给 Home Assistant

    obj["id"] = _id;                   // Sensor ID | 传感器 ID
    obj["name"] = _name;               // Display name | 显示名称
    obj["type"] = "sensor";            // Entity type (fixed as sensor) | 实体类型（固定为 sensor）

    // Device class (e.g., temperature, humidity)
    // 设备类别（如 temperature, humidity）
    if (_deviceClass.length() > 0) {
        obj["device_class"] = _deviceClass;
    }

    // Unit (e.g., °C, %)
    // 单位（如 °C, %）
    if (_unit.length() > 0) {
        obj["unit_of_measurement"] = _unit;
    }

    // State class (e.g., measurement, total)
    // 状态类别（如 measurement, total）
    obj["state_class"] = _stateClass;

    // Display precision | 显示精度
    obj["precision"] = _precision;

    // Icon | 图标
    if (_icon.length() > 0) {
        obj["icon"] = _icon;
    }

    // Current value (if set) | 当前值（如果已设置）
    if (_hasValue) {
        obj["state"] = _value;
    }
}

void SeeedHASensor::_notifyChange() {
    // Notify main class that sensor value is updated
    // 通知主类，传感器值已更新
    if (_ha != nullptr) {
        _ha->_notifySensorChange(_id);
    }
}

// =============================================================================
// SeeedHASwitch Implementation | SeeedHASwitch 实现
// =============================================================================

SeeedHASwitch::SeeedHASwitch(
    const String& id,
    const String& name,
    const String& icon
) :
    _id(id),
    _name(name),
    _icon(icon),
    _state(false),
    _callback(nullptr),
    _ha(nullptr)
{
    // Constructor initialization complete | 构造函数初始化完成
}

void SeeedHASwitch::setState(bool state) {
    // If state unchanged, do nothing
    // 如果状态没有变化，不做任何事
    if (_state == state) {
        return;
    }

    // Update state | 更新状态
    _state = state;

    // Notify main class that state is updated (will send to HA)
    // 通知主类，状态已更新（会发送到 HA）
    _notifyChange();
}

void SeeedHASwitch::toggle() {
    // Toggle state | 切换状态
    setState(!_state);
}

void SeeedHASwitch::onStateChange(SwitchCallback callback) {
    // Register callback | 注册回调函数
    _callback = callback;
}

void SeeedHASwitch::setIcon(const String& icon) {
    _icon = icon;
}

void SeeedHASwitch::toJson(JsonObject& obj) const {
    // Convert switch info to JSON format
    // This JSON is sent to Home Assistant
    // 将开关信息转换为 JSON 格式
    // 这个 JSON 会发送给 Home Assistant

    obj["id"] = _id;           // Switch ID | 开关 ID
    obj["name"] = _name;       // Display name | 显示名称
    obj["type"] = "switch";    // Entity type (fixed as switch) | 实体类型（固定为 switch）
    obj["state"] = _state;     // Current state | 当前状态

    // Icon | 图标
    if (_icon.length() > 0) {
        obj["icon"] = _icon;
    }
}

void SeeedHASwitch::_handleCommand(bool state) {
    // Handle command from HA | 处理来自 HA 的命令

    // Update state first (but don't notify to avoid loop)
    // 先更新状态（但不通知，避免循环）
    _state = state;

    // If callback exists, call it for hardware operation
    // 如果有回调，调用回调让用户处理硬件操作
    if (_callback != nullptr) {
        _callback(state);
    }

    // Notify main class to send state confirmation to HA
    // 通知主类发送状态确认给 HA
    _notifyChange();
}

void SeeedHASwitch::_notifyChange() {
    // Notify main class that switch state is updated
    // 通知主类，开关状态已更新
    if (_ha != nullptr) {
        _ha->_notifySwitchChange(_id);
    }
}

// =============================================================================
// SeeedHAState Implementation | SeeedHAState 实现
// =============================================================================

SeeedHAState::SeeedHAState(const String& entityId) :
    _entityId(entityId),
    _state(""),
    _friendlyName(""),
    _unit(""),
    _deviceClass(""),
    _hasValue(false),
    _lastUpdate(0)
{
    // Constructor initialization complete | 构造函数初始化完成
}

float SeeedHAState::getFloat() const {
    if (!_hasValue) return 0;
    return _state.toFloat();
}

int SeeedHAState::getInt() const {
    if (!_hasValue) return 0;
    return _state.toInt();
}

bool SeeedHAState::getBool() const {
    if (!_hasValue) return false;
    
    // Check common "on" states | 检查常见的 "开" 状态
    String s = _state;
    s.toLowerCase();
    return (s == "on" || s == "true" || s == "1" || 
            s == "home" || s == "open" || s == "yes");
}

void SeeedHAState::_updateState(const String& state, JsonObject& attributes) {
    _state = state;
    _hasValue = true;
    _lastUpdate = millis();
    
    // Update attributes | 更新属性
    if (attributes.containsKey("friendly_name")) {
        _friendlyName = attributes["friendly_name"].as<String>();
    }
    if (attributes.containsKey("unit_of_measurement")) {
        _unit = attributes["unit_of_measurement"].as<String>();
    }
    if (attributes.containsKey("device_class")) {
        _deviceClass = attributes["device_class"].as<String>();
    }
}

// =============================================================================
// SeeedHADiscovery Implementation | SeeedHADiscovery 实现
// =============================================================================

SeeedHADiscovery::SeeedHADiscovery() :
    _deviceName("Seeed HA Device"),
    _deviceModel("ESP32"),
    _deviceVersion(SEEED_HA_DISCOVERY_VERSION),
    _httpServer(nullptr),
    _wsServer(nullptr),
    _wsClientConnected(false),
    _provisioning(nullptr),
    _haStateCallback(nullptr),
    _debug(false),
    _lastHeartbeat(0),
    _deviceId("")  // Will be generated in begin() after WiFi init
{
    // Device ID will be generated in begin() after WiFi is initialized
    // 设备 ID 将在 begin() 中 WiFi 初始化后生成
}

SeeedHADiscovery::~SeeedHADiscovery() {
    // Cleanup HTTP server | 清理 HTTP 服务器
    if (_httpServer != nullptr) {
        _httpServer->stop();
        delete _httpServer;
    }

    // Cleanup WebSocket server | 清理 WebSocket 服务器
    if (_wsServer != nullptr) {
        _wsServer->close();
        delete _wsServer;
    }

    // Cleanup WiFi provisioning | 清理 WiFi 配网
    if (_provisioning != nullptr) {
        delete _provisioning;
    }

    // Cleanup sensors | 清理传感器
    for (auto sensor : _sensors) {
        delete sensor;
    }
    _sensors.clear();

    // Cleanup switches | 清理开关
    for (auto sw : _switches) {
        delete sw;
    }
    _switches.clear();

    // Cleanup HA states | 清理 HA 状态
    for (auto& pair : _haStates) {
        delete pair.second;
    }
    _haStates.clear();
}

void SeeedHADiscovery::setDeviceInfo(const String& name, const String& model, const String& version) {
    _deviceName = name;
    _deviceModel = model;
    _deviceVersion = version;
}

void SeeedHADiscovery::enableDebug(bool enable) {
    _debug = enable;
}

bool SeeedHADiscovery::begin(const char* ssid, const char* password) {
    _log("====================================");
    _log("Seeed HA Discovery starting...");
    _log("====================================");

    // -------------------------------------------------------------------------
    // Step 1: Connect WiFi | 步骤 1: 连接 WiFi
    // -------------------------------------------------------------------------
    _log("Connecting to WiFi: " + String(ssid));

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Wait for connection (max 30 seconds) | 等待连接（最多 30 秒）
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
        delay(500);
        if (_debug) {
            Serial.print(".");
        }
        attempts++;
    }

    if (_debug) {
        Serial.println();
    }

    // Check connection result | 检查连接结果
    if (WiFi.status() != WL_CONNECTED) {
        _log("WiFi connection failed!");
        return false;
    }

    // Generate device ID after WiFi is initialized (MAC address is now valid)
    // 在 WiFi 初始化后生成设备 ID（此时 MAC 地址有效）
    _deviceId = _generateDeviceId();

    _log("WiFi connected!");
    _log("IP Address: " + WiFi.localIP().toString());
    _log("MAC Address: " + WiFi.macAddress());
    _log("Device ID: " + _deviceId);

    // -------------------------------------------------------------------------
    // Step 2: Start mDNS service (for auto discovery)
    // 步骤 2: 启动 mDNS 服务（用于设备自动发现）
    // -------------------------------------------------------------------------
    _setupMDNS();

    // -------------------------------------------------------------------------
    // Step 3: Start HTTP server (device info API)
    // 步骤 3: 启动 HTTP 服务器（提供设备信息接口）
    // -------------------------------------------------------------------------
    _setupHTTP();

    // -------------------------------------------------------------------------
    // Step 4: Start WebSocket server (real-time communication)
    // 步骤 4: 启动 WebSocket 服务器（用于实时通信）
    // -------------------------------------------------------------------------
    _setupWebSocket();

    _log("====================================");
    _log("All services started!");
    _log("Open in browser: http://" + WiFi.localIP().toString());
    _log("====================================");

    return true;
}

bool SeeedHADiscovery::beginWithProvisioning(const String& apSSID) {
    _log("====================================");
    _log("Seeed HA Discovery starting with provisioning...");
    _log("====================================");

    // Create provisioning instance if not exists
    // 如果不存在则创建配网实例
    if (_provisioning == nullptr) {
        _provisioning = new SeeedWiFiProvisioning();
    }

    // Configure provisioning | 配置配网
    _provisioning->setAPSSID(apSSID);
    _provisioning->enableDebug(_debug);

    // Try to connect using saved credentials or start AP mode
    // 尝试使用保存的凭据连接或启动 AP 模式
    bool connected = _provisioning->begin();

    if (connected) {
        // WiFi connected, start HA services | WiFi 已连接，启动 HA 服务
        _log("WiFi connected via provisioning!");
        
        // Generate device ID after WiFi is initialized (MAC address is now valid)
        // 在 WiFi 初始化后生成设备 ID（此时 MAC 地址有效）
        _deviceId = _generateDeviceId();
        
        _log("IP Address: " + WiFi.localIP().toString());
        _log("MAC Address: " + WiFi.macAddress());
        _log("Device ID: " + _deviceId);

        // Start mDNS service | 启动 mDNS 服务
        _setupMDNS();

        // Start HTTP server | 启动 HTTP 服务器
        _setupHTTP();

        // Start WebSocket server | 启动 WebSocket 服务器
        _setupWebSocket();

        _log("====================================");
        _log("All services started!");
        _log("Open in browser: http://" + WiFi.localIP().toString());
        _log("====================================");

        return true;
    } else {
        // AP mode is active for configuration
        // AP 模式已激活用于配置
        _log("====================================");
        _log("AP Mode Active for WiFi Configuration");
        _log("Connect to WiFi: " + apSSID);
        _log("Open browser: http://192.168.4.1");
        _log("====================================");

        return false;
    }
}

bool SeeedHADiscovery::isProvisioningActive() const {
    if (_provisioning != nullptr) {
        return _provisioning->isAPModeActive();
    }
    return false;
}

void SeeedHADiscovery::clearWiFiCredentials() {
    if (_provisioning != nullptr) {
        _provisioning->clearCredentials();
    } else {
        // Create temporary instance to clear credentials
        // 创建临时实例来清除凭据
        SeeedWiFiProvisioning temp;
        temp.clearCredentials();
    }
    _log("WiFi credentials cleared");
}

void SeeedHADiscovery::enableResetButton(int pin, bool activeLow) {
    if (_provisioning != nullptr) {
        _provisioning->enableResetButton(pin, activeLow);
        _log("Reset button enabled on GPIO" + String(pin) + 
             " - long press 6s to reset WiFi");
    } else {
        _log("Warning: WiFi provisioning not initialized, reset button not enabled");
    }
}

void SeeedHADiscovery::disableResetButton() {
    if (_provisioning != nullptr) {
        _provisioning->disableResetButton();
        _log("Reset button disabled");
    }
}

void SeeedHADiscovery::_setupMDNS() {
    // Generate hostname (based on device ID) | 生成主机名（基于设备 ID）
    String hostname = "seeed-ha-" + _deviceId;
    hostname.toLowerCase();

    _log("Starting mDNS service: " + hostname + ".local");

    if (MDNS.begin(hostname.c_str())) {
        // Register _seeed_ha._tcp service for Home Assistant discovery
        // 注册 _seeed_ha._tcp 服务，这样 Home Assistant 就能发现设备
        MDNS.addService("seeed_ha", "tcp", SEEED_HA_WS_PORT);

        // Add TXT records with device info | 添加 TXT 记录，包含设备信息
        MDNS.addServiceTxt("seeed_ha", "tcp", "id", _deviceId);
        MDNS.addServiceTxt("seeed_ha", "tcp", "name", _deviceName);
        MDNS.addServiceTxt("seeed_ha", "tcp", "model", _deviceModel);
        MDNS.addServiceTxt("seeed_ha", "tcp", "version", _deviceVersion);
        // Add MAC address for reliable device identification
        // 添加 MAC 地址用于可靠的设备识别
        MDNS.addServiceTxt("seeed_ha", "tcp", "mac", WiFi.macAddress());

        _log("mDNS service started");
    } else {
        _log("mDNS startup failed!");
    }
}

void SeeedHADiscovery::_setupHTTP() {
    _httpServer = new WebServer(SEEED_HA_HTTP_PORT);

    // Register route handlers | 注册路由处理器

    // Home page - display device status page | 主页 - 显示设备状态页面
    _httpServer->on("/", HTTP_GET, [this]() {
        _handleHTTPRoot();
    });

    // Device info API - return JSON device info | 设备信息接口 - 返回 JSON 格式的设备信息
    _httpServer->on("/info", HTTP_GET, [this]() {
        _handleHTTPInfo();
    });

    // Start server | 启动服务器
    _httpServer->begin();
    _log("HTTP server started, port: " + String(SEEED_HA_HTTP_PORT));
}

void SeeedHADiscovery::_setupWebSocket() {
    _wsServer = new WebSocketsServer(SEEED_HA_WS_PORT);

    // Register WebSocket event handler | 注册 WebSocket 事件处理器
    _wsServer->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        _handleWSEvent(num, type, payload, length);
    });

    // Start server | 启动服务器
    _wsServer->begin();
    _log("WebSocket server started, port: " + String(SEEED_HA_WS_PORT));
}

void SeeedHADiscovery::_handleHTTPRoot() {
    // Generate a simple status page | 生成一个简单的状态页面
    String html = R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Seeed HA Discovery</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            padding: 20px;
            color: #eee;
        }
        .container { max-width: 600px; margin: 0 auto; }
        h1 {
            color: #00d9ff;
            margin-bottom: 20px;
            font-size: 2em;
        }
        .card {
            background: rgba(255,255,255,0.1);
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
            backdrop-filter: blur(10px);
        }
        .card h2 {
            color: #00d9ff;
            margin-bottom: 15px;
            font-size: 1.2em;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 10px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        .info-row:last-child { border-bottom: none; }
        .label { color: #888; }
        .value { color: #fff; font-weight: 500; }
        .sensor {
            background: rgba(0,217,255,0.1);
            border-radius: 8px;
            padding: 15px;
            margin-bottom: 10px;
        }
        .sensor-name {
            font-weight: 600;
            color: #00d9ff;
        }
        .sensor-value {
            font-size: 1.5em;
            margin-top: 5px;
        }
        .sensor-unit {
            color: #888;
            font-size: 0.8em;
        }
        .status {
            display: inline-block;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.9em;
        }
        .status.online, .status.on {
            background: rgba(0,255,136,0.2);
            color: #00ff88;
        }
        .status.off {
            background: rgba(255,100,100,0.2);
            color: #ff6464;
        }
        .footer {
            text-align: center;
            color: #666;
            font-size: 0.9em;
            margin-top: 20px;
        }
        .logo {
            color: #00d9ff;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌱 Seeed HA Discovery</h1>

        <div class="card">
            <h2>Device Info</h2>
            <div class="info-row">
                <span class="label">Device Name</span>
                <span class="value">)" + _deviceName + R"(</span>
            </div>
            <div class="info-row">
                <span class="label">Device Model</span>
                <span class="value">)" + _deviceModel + R"(</span>
            </div>
            <div class="info-row">
                <span class="label">Firmware Version</span>
                <span class="value">)" + _deviceVersion + R"(</span>
            </div>
            <div class="info-row">
                <span class="label">Device ID</span>
                <span class="value">)" + _deviceId + R"(</span>
            </div>
            <div class="info-row">
                <span class="label">IP Address</span>
                <span class="value">)" + WiFi.localIP().toString() + R"(</span>
            </div>
            <div class="info-row">
                <span class="label">Home Assistant</span>
                <span class="status )" + String(_wsClientConnected ? "online" : "") + R"(">
                    )" + String(_wsClientConnected ? "Connected" : "Waiting") + R"(
                </span>
            </div>
        </div>

        <div class="card">
            <h2>Sensors</h2>)";

    // Add sensor list | 添加传感器列表
    if (_sensors.empty()) {
        html += R"(
            <p style="color: #888;">No sensors</p>)";
    } else {
        for (auto sensor : _sensors) {
            html += R"(
            <div class="sensor">
                <div class="sensor-name">)" + sensor->getName() + R"(</div>
                <div class="sensor-value">
                    )" + String(sensor->getValue(), sensor->getPrecision()) + R"(
                    <span class="sensor-unit">)" + sensor->getUnit() + R"(</span>
                </div>
            </div>)";
        }
    }

    html += R"(
        </div>

        <div class="card">
            <h2>Switches</h2>)";

    // Add switch list | 添加开关列表
    if (_switches.empty()) {
        html += R"(
            <p style="color: #888;">No switches</p>)";
    } else {
        for (auto sw : _switches) {
            String stateClass = sw->getState() ? "on" : "off";
            String stateText = sw->getState() ? "ON" : "OFF";
            html += R"(
            <div class="sensor">
                <div class="sensor-name">)" + sw->getName() + R"(</div>
                <div class="sensor-value">
                    <span class="status )" + stateClass + R"(">)" + stateText + R"(</span>
                </div>
            </div>)";
        }
    }

    html += R"(
        </div>

        <div class="footer">
            <span class="logo">Seeed Studio</span> | Seeed HA Discovery v)" + String(SEEED_HA_DISCOVERY_VERSION) + R"(
        </div>
    </div>
</body>
</html>)";

    _httpServer->send(200, "text/html; charset=utf-8", html);
}

void SeeedHADiscovery::_handleHTTPInfo() {
    // Return JSON device info
    // Home Assistant calls this API to get device info
    // 返回 JSON 格式的设备信息
    // Home Assistant 会调用这个接口获取设备信息

    JsonDocument doc;

    doc["device_id"] = _deviceId;
    doc["name"] = _deviceName;
    doc["model"] = _deviceModel;
    doc["version"] = _deviceVersion;
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["rssi"] = WiFi.RSSI();
    // Add connection status - indicates if device is already connected to an HA instance
    // 添加连接状态 - 表示设备是否已连接到某个 HA 实例
    doc["connected"] = _wsClientConnected;

    String response;
    serializeJson(doc, response);

    _httpServer->send(200, "application/json", response);
}

void SeeedHADiscovery::_handleWSEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            // Client disconnected | 客户端断开连接
            _log("WebSocket client #" + String(num) + " disconnected");
            _wsClientConnected = false;
            break;

        case WStype_CONNECTED: {
            // New client connected | 新客户端连接
            IPAddress ip = _wsServer->remoteIP(num);
            _log("WebSocket client #" + String(num) + " connected from " + ip.toString());
            _wsClientConnected = true;

            // Send discovery info to new client | 向新客户端发送发现信息
            _sendDiscovery(num);
            break;
        }

        case WStype_TEXT: {
            // Received text message | 收到文本消息
            String message = String((char*)payload);

            // Parse JSON | 解析 JSON
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, message);

            if (error) {
                _log("JSON parse error: " + String(error.c_str()));
                return;
            }

            // Get message type | 获取消息类型
            String msgType = doc["type"].as<String>();

            // Skip logging heartbeat traffic to keep the console readable.
            // 跳过心跳消息日志，保持串口输出清爽。
            if (msgType != "ping" && msgType != "pong") {
                _log("Message received: " + message);
            }

            if (msgType == "ping") {
                // Heartbeat request, reply pong | 心跳请求，回复 pong
                JsonDocument response;
                response["type"] = "pong";
                response["timestamp"] = doc["timestamp"];

                String responseStr;
                serializeJson(response, responseStr);
                _wsServer->sendTXT(num, responseStr);
            }
            else if (msgType == "discovery") {
                // Discovery request, send entity list | 发现请求，发送实体列表
                _sendDiscovery(num);
            }
            else if (msgType == "command") {
                // Control command from HA | 来自 HA 的控制命令
                // Format: {type: "command", entity_id: "led", command: "turn_on"} or
                // 格式: {type: "command", entity_id: "led", state: true}
                _handleCommand(doc);
            }
            else if (msgType == "ha_state") {
                // HA entity state push | HA 实体状态推送
                // Format: {type: "ha_state", entity_id: "sensor.xxx", state: "25.5", attributes: {...}}
                // 格式: {type: "ha_state", entity_id: "sensor.xxx", state: "25.5", attributes: {...}}
                _handleHAState(doc);
            }
            else if (msgType == "ha_state_clear") {
                // HA entity state clear | HA 实体状态清除
                // Format: {type: "ha_state_clear"}
                // 格式: {type: "ha_state_clear"}
                _log("Received HA state clear command");
                clearHAStates();
            }
            else {
                auto callback = _protocolMessageCallbacks.find(msgType);
                if (callback != _protocolMessageCallbacks.end()) {
                    callback->second(doc);
                }
            }
            break;
        }

        default:
            break;
    }
}

void SeeedHADiscovery::_sendDiscovery(uint8_t clientNum) {
    // Build discovery message | 构建发现消息
    JsonDocument doc;
    doc["type"] = "discovery";

    JsonArray entities = doc["entities"].to<JsonArray>();

    // Add all sensors | 添加所有传感器
    for (auto sensor : _sensors) {
        JsonObject obj = entities.add<JsonObject>();
        sensor->toJson(obj);
    }

    // Add all switches | 添加所有开关
    for (auto sw : _switches) {
        JsonObject obj = entities.add<JsonObject>();
        sw->toJson(obj);
    }

    // Append entities provided by optional protocol modules
    // 追加可选协议模块提供的实体
    for (auto& callback : _discoveryCallbacks) {
        callback(entities);
    }

    // Serialize and send | 序列化并发送
    String message;
    serializeJson(doc, message);

    if (clientNum == 255) {
        // Broadcast to all clients | 广播给所有客户端
        _broadcastMessage(message);
    } else {
        // Send to specific client | 发送给指定客户端
        _wsServer->sendTXT(clientNum, message);
    }

    _log("Sent discovery info: " + String(_sensors.size()) + " sensors, " +
         String(_switches.size()) + " switches");
}

void SeeedHADiscovery::_sendSensorState(const String& sensorId, uint8_t clientNum) {
    // Find sensor | 查找传感器
    SeeedHASensor* sensor = nullptr;
    for (auto s : _sensors) {
        if (s->getId() == sensorId) {
            sensor = s;
            break;
        }
    }

    if (sensor == nullptr) {
        return;
    }

    // Build state update message | 构建状态更新消息
    JsonDocument doc;
    doc["type"] = "state";
    doc["entity_id"] = sensorId;
    doc["state"] = sensor->getValue();

    // Add extra attributes | 可以添加额外属性
    JsonObject attrs = doc["attributes"].to<JsonObject>();
    attrs["unit_of_measurement"] = sensor->getUnit();
    attrs["device_class"] = sensor->getDeviceClass();

    // Serialize and send | 序列化并发送
    String message;
    serializeJson(doc, message);

    if (clientNum == 255) {
        _broadcastMessage(message);
    } else {
        _wsServer->sendTXT(clientNum, message);
    }

    _log("Sent state update: " + sensorId + " = " + String(sensor->getValue()));
}

void SeeedHADiscovery::_broadcastMessage(const String& message) {
    // WebSockets library needs non-const reference, so create copy
    // WebSockets 库的 broadcastTXT 需要非 const 引用，所以创建副本
    String msg = message;
    _wsServer->broadcastTXT(msg);
}

SeeedHASensor* SeeedHADiscovery::addSensor(
    const String& id,
    const String& name,
    const String& deviceClass,
    const String& unit
) {
    // Create new sensor | 创建新传感器
    SeeedHASensor* sensor = new SeeedHASensor(id, name, deviceClass, unit);
    sensor->_ha = this;

    // Add to list | 添加到列表
    _sensors.push_back(sensor);

    _log("Added sensor: " + name + " (ID: " + id + ")");

    return sensor;
}

SeeedHASwitch* SeeedHADiscovery::addSwitch(
    const String& id,
    const String& name,
    const String& icon
) {
    // Create new switch | 创建新开关
    SeeedHASwitch* sw = new SeeedHASwitch(id, name, icon);
    sw->_ha = this;

    // Add to list | 添加到列表
    _switches.push_back(sw);

    _log("Added switch: " + name + " (ID: " + id + ")");

    return sw;
}

void SeeedHADiscovery::_notifySensorChange(const String& sensorId) {
    // When sensor value changes, send state update
    // 当传感器值变化时，发送状态更新
    if (_wsClientConnected) {
        _sendSensorState(sensorId);
    }
}

void SeeedHADiscovery::_notifySwitchChange(const String& switchId) {
    // When switch state changes, send state update
    // 当开关状态变化时，发送状态更新
    if (_wsClientConnected) {
        _sendSwitchState(switchId);
    }
}

void SeeedHADiscovery::_sendSwitchState(const String& switchId, uint8_t clientNum) {
    // Find switch | 查找开关
    SeeedHASwitch* sw = nullptr;
    for (auto s : _switches) {
        if (s->getId() == switchId) {
            sw = s;
            break;
        }
    }

    if (sw == nullptr) {
        return;
    }

    // Build state update message | 构建状态更新消息
    JsonDocument doc;
    doc["type"] = "state";
    doc["entity_id"] = switchId;
    doc["state"] = sw->getState();

    // Serialize and send | 序列化并发送
    String message;
    serializeJson(doc, message);

    if (clientNum == 255) {
        _broadcastMessage(message);
    } else {
        _wsServer->sendTXT(clientNum, message);
    }

    _log("Sent switch state: " + switchId + " = " + String(sw->getState() ? "ON" : "OFF"));
}

void SeeedHADiscovery::_handleCommand(JsonDocument& doc) {
    // Handle control command from Home Assistant
    // 处理来自 Home Assistant 的控制命令
    // Format 1: {type: "command", entity_id: "led", command: "turn_on"}
    // Format 2: {type: "command", entity_id: "led", state: true}
    // 格式 1: {type: "command", entity_id: "led", command: "turn_on"}
    // 格式 2: {type: "command", entity_id: "led", state: true}

    String entityId = doc["entity_id"].as<String>();

    if (entityId.length() == 0) {
        _log("Command error: missing entity_id");
        return;
    }

    // Determine target state | 确定目标状态
    bool targetState = false;

    if (doc["command"].is<String>()) {
        // Format 1: Use command string | 格式 1: 使用命令字符串
        String command = doc["command"].as<String>();
        if (command == "turn_on") {
            targetState = true;
        } else if (command == "turn_off") {
            targetState = false;
        } else if (command == "toggle") {
            // Need to find switch to get current state
            // 需要先找到开关获取当前状态
            for (auto sw : _switches) {
                if (sw->getId() == entityId) {
                    targetState = !sw->getState();
                    break;
                }
            }
        } else {
            _log("Unknown command: " + command);
            return;
        }
    } else if (doc["state"].is<bool>()) {
        // Format 2: Use state value directly | 格式 2: 直接使用状态值
        targetState = doc["state"].as<bool>();
    } else {
        _log("Command error: missing command or state");
        return;
    }

    // Find and execute command | 查找并执行命令
    for (auto sw : _switches) {
        if (sw->getId() == entityId) {
            _log("Executing command: " + entityId + " -> " + String(targetState ? "ON" : "OFF"));
            sw->_handleCommand(targetState);
            return;
        }
    }

    _log("Switch not found: " + entityId);
}

void SeeedHADiscovery::_handleHAState(JsonDocument& doc) {
    // Handle HA entity state push from Home Assistant
    // 处理来自 Home Assistant 的实体状态推送
    // Format: {type: "ha_state", entity_id: "sensor.xxx", state: "25.5", attributes: {...}}
    // 格式: {type: "ha_state", entity_id: "sensor.xxx", state: "25.5", attributes: {...}}

    String entityId = doc["entity_id"].as<String>();
    String state = doc["state"].as<String>();

    if (entityId.length() == 0) {
        _log("HA state error: missing entity_id");
        return;
    }

    _log("Received HA state: " + entityId + " = " + state);

    // Get or create SeeedHAState object | 获取或创建 SeeedHAState 对象
    SeeedHAState* haState = nullptr;
    auto it = _haStates.find(entityId);
    if (it != _haStates.end()) {
        haState = it->second;
    } else {
        // Create new state object | 创建新的状态对象
        if (_haStates.size() >= SEEED_HA_MAX_SUBSCRIBED_ENTITIES) {
            _log("HA state error: max entities reached");
            return;
        }
        haState = new SeeedHAState(entityId);
        _haStates[entityId] = haState;
        _log("Created new HA state for: " + entityId);
    }

    // Update state | 更新状态
    JsonObject attrs = doc["attributes"].as<JsonObject>();
    haState->_updateState(state, attrs);

    // Call user callback if registered | 如果注册了回调，调用用户回调
    if (_haStateCallback != nullptr) {
        _haStateCallback(entityId.c_str(), state.c_str(), attrs);
    }
}

void SeeedHADiscovery::onHAState(HAStateCallback callback) {
    _haStateCallback = callback;
    _log("HA state callback registered");
}

SeeedHAState* SeeedHADiscovery::getHAState(const String& entityId) {
    auto it = _haStates.find(entityId);
    if (it != _haStates.end()) {
        return it->second;
    }
    return nullptr;
}

void SeeedHADiscovery::clearHAStates() {
    // Clear all subscribed HA states | 清除所有订阅的 HA 状态
    _log("Clearing all HA states, count: " + String(_haStates.size()));
    
    for (auto& pair : _haStates) {
        delete pair.second;
    }
    _haStates.clear();
    
    _log("HA states cleared");
}

void SeeedHADiscovery::onProtocolMessage(
    const String& type,
    HAProtocolMessageCallback callback
) {
    _protocolMessageCallbacks[type] = callback;
}

void SeeedHADiscovery::onDiscovery(HADiscoveryCallback callback) {
    _discoveryCallbacks.push_back(callback);
}

void SeeedHADiscovery::sendProtocolMessage(JsonDocument& doc, uint8_t clientNum) {
    String message;
    serializeJson(doc, message);

    if (clientNum == 255) {
        _broadcastMessage(message);
    } else if (_wsServer != nullptr) {
        _wsServer->sendTXT(clientNum, message);
    }
}

void SeeedHADiscovery::handle() {
    // Process provisioning and its reset button in every network state
    // 在所有网络状态下处理配网和重置按钮
    if (_provisioning != nullptr) {
        _provisioning->handle();
        if (_provisioning->isAPModeActive()) {
            return;
        }
    }

    // Handle HTTP requests | 处理 HTTP 请求
    if (_httpServer != nullptr) {
        _httpServer->handleClient();
    }

    // Handle WebSocket | 处理 WebSocket
    if (_wsServer != nullptr) {
        _wsServer->loop();
    }

    // Periodic heartbeat (every 30 seconds) | 定期心跳（每 30 秒）
    unsigned long now = millis();
    if (now - _lastHeartbeat > 30000) {
        _lastHeartbeat = now;

        if (_wsClientConnected) {
            // Send heartbeat | 发送心跳
            JsonDocument doc;
            doc["type"] = "ping";
            doc["timestamp"] = now;

            String message;
            serializeJson(doc, message);
            _broadcastMessage(message);
        }
    }
}

bool SeeedHADiscovery::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool SeeedHADiscovery::isHAConnected() const {
    return _wsClientConnected;
}

void SeeedHADiscovery::notifySleep() {
    // 通知 HA 设备即将进入休眠模式
    // Notify HA that device is about to enter sleep mode
    if (_wsClientConnected && _wsServer) {
        JsonDocument doc;
        doc["type"] = "sleep";
        doc["timestamp"] = millis();
        
        String message;
        serializeJson(doc, message);
        _broadcastMessage(message);
        
        _log("Notified HA: entering sleep mode");
        
        // 给 WebSocket 一点时间发送消息
        // Give WebSocket a moment to send the message
        delay(50);
    }
}

IPAddress SeeedHADiscovery::getLocalIP() const {
    return WiFi.localIP();
}

String SeeedHADiscovery::_generateDeviceId() {
    // Generate unique device ID using MAC address
    // 使用 MAC 地址生成唯一设备 ID
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char id[13];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return String(id);
}

void SeeedHADiscovery::_log(const String& message) {
    if (_debug) {
        Serial.println("[SeeedHA] " + message);
    }
}
