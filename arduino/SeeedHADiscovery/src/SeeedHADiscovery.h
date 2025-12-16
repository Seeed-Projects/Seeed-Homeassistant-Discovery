/**
 * ============================================================================
 * Seeed Home Assistant Discovery - Arduino library for connecting ESP32 to Home Assistant
 * Seeed Home Assistant Discovery - ESP32 连接 Home Assistant 的 Arduino 库
 * ============================================================================
 *
 * This library enables ESP32 devices to easily connect to Home Assistant:
 * 这个库让 ESP32 设备能够轻松连接到 Home Assistant：
 * - Supports ESP32-C3, C6, S3 and other models
 *   支持 ESP32-C3、C6、S3 等型号
 * - Automatically discovered by Home Assistant via mDNS
 *   通过 mDNS 自动被 Home Assistant 发现
 * - Real-time sensor data push using WebSocket
 *   使用 WebSocket 实时推送传感器数据
 * - Simple and easy-to-use API
 *   简单易用的 API
 *
 * Usage:
 * 使用方法：
 * 1. Create SeeedHADiscovery instance
 *    创建 SeeedHADiscovery 实例
 * 2. Call begin() to connect WiFi
 *    调用 begin() 连接 WiFi
 * 3. Use addSensor() to add sensors
 *    使用 addSensor() 添加传感器
 * 4. Call handle() in loop() and update sensor values
 *    在 loop() 中调用 handle() 和更新传感器值
 *
 * Example:
 * 示例：
 * ```cpp
 * SeeedHADiscovery ha;
 * SeeedHASensor* tempSensor;
 *
 * void setup() {
 *     ha.begin("WiFi_SSID", "WiFi_Password");
 *     tempSensor = ha.addSensor("temp", "Temperature", "temperature", "°C");
 * }
 *
 * void loop() {
 *     ha.handle();
 *     tempSensor->setValue(25.5);
 * }
 * ```
 *
 * @author limengdu
 * @version 1.3.0
 * @license MIT
 */

#ifndef SEEED_HA_DISCOVERY_H
#define SEEED_HA_DISCOVERY_H

// =============================================================================
// Dependencies | 依赖库
// =============================================================================

#include <Arduino.h>       // Arduino core library | Arduino 核心库
#include <WiFi.h>          // ESP32 WiFi library | ESP32 WiFi 库
#include <WebServer.h>     // HTTP server (for device info) | HTTP 服务器（用于提供设备信息）
#include <WebSocketsServer.h>  // WebSocket server (for real-time communication) | WebSocket 服务器（用于实时通信）
#include <ESPmDNS.h>       // mDNS service (for device discovery) | mDNS 服务（用于设备发现）
#include <ArduinoJson.h>   // JSON processing library | JSON 处理库
#include <vector>          // Dynamic array | 动态数组
#include <map>             // Key-value container | 键值对容器

// Forward declaration for WiFi provisioning | WiFi 配网前向声明
class SeeedWiFiProvisioning;

// =============================================================================
// Constants | 常量定义
// =============================================================================

// Library version | 库版本号
#define SEEED_HA_DISCOVERY_VERSION "1.5.2"

// Maximum number of subscribed HA entities | 最大订阅 HA 实体数量
#define SEEED_HA_MAX_SUBSCRIBED_ENTITIES 20

// Default ports | 默认端口
#define SEEED_HA_HTTP_PORT 80   // HTTP server port (for device info API) | HTTP 服务器端口（用于设备信息接口）
#define SEEED_HA_WS_PORT 81     // WebSocket port (for real-time communication) | WebSocket 端口（用于实时通信）

// =============================================================================
// Forward Declarations | 前向声明
// =============================================================================

class SeeedHADiscovery;
class SeeedHASensor;
class SeeedHASwitch;

// =============================================================================
// Callback Type Definitions | 回调函数类型定义
// =============================================================================

/**
 * Switch state change callback type
 * 开关状态变化回调函数类型
 *
 * @param state New switch state (true = ON, false = OFF)
 *              新的开关状态（true = 开, false = 关）
 */
typedef void (*SwitchCallback)(bool state);

/**
 * HA state change callback type
 * HA 状态变化回调函数类型
 *
 * Called when a subscribed Home Assistant entity's state changes.
 * 当订阅的 Home Assistant 实体状态变化时调用。
 *
 * @param entityId The entity ID (e.g., "sensor.living_room_temperature")
 *                 实体 ID（如 "sensor.living_room_temperature"）
 * @param state The new state value as string
 *              新的状态值（字符串形式）
 * @param attributes JSON object containing entity attributes
 *                   包含实体属性的 JSON 对象
 */
typedef void (*HAStateCallback)(const char* entityId, const char* state, JsonObject& attributes);

// =============================================================================
// SeeedHASensor - Sensor Class | 传感器类
// =============================================================================

/**
 * Sensor class - represents a numeric sensor
 * 传感器类 - 代表一个数值型传感器
 *
 * Used to report measurement values to Home Assistant.
 * 用于向 Home Assistant 报告测量值，如温度、湿度等。
 *
 * Supported attributes:
 * 支持的属性：
 * - device_class: Device class (temperature, humidity, etc.)
 *   设备类别（temperature, humidity 等）
 * - unit_of_measurement: Unit (°C, %, etc.)
 *   单位（°C, % 等）
 * - state_class: State class (measurement, total, etc.)
 *   状态类别（measurement, total 等）
 * - precision: Decimal precision
 *   小数精度
 * - icon: Icon (mdi:xxx format)
 *   图标（mdi:xxx 格式）
 */
class SeeedHASensor {
public:
    /**
     * Constructor | 构造函数
     *
     * @param id Sensor ID (unique identifier, e.g., "temperature")
     *           传感器 ID（唯一标识符，如 "temperature"）
     * @param name Display name (e.g., "Temperature")
     *             显示名称（如 "温度"）
     * @param deviceClass Device class (e.g., "temperature")
     *                    设备类别（如 "temperature"）
     * @param unit Unit (e.g., "°C")
     *             单位（如 "°C"）
     */
    SeeedHASensor(
        const String& id,
        const String& name,
        const String& deviceClass = "",
        const String& unit = ""
    );

    // =========================================================================
    // Setters | 设置方法
    // =========================================================================

    /**
     * Set current sensor value
     * 设置传感器的当前值
     *
     * After calling this method, new value is automatically pushed to Home Assistant.
     * 调用此方法后，新值会自动推送到 Home Assistant。
     *
     * @param value Sensor value | 传感器值
     */
    void setValue(float value);

    /**
     * Set state class
     * 设置状态类别
     *
     * @param stateClass State class:
     *                   状态类别：
     *        - "measurement": Measurement value (default) | 测量值（默认）
     *        - "total": Total value | 总计值
     *        - "total_increasing": Only increasing total | 只增不减的总计值
     */
    void setStateClass(const String& stateClass);

    /**
     * Set display precision
     * 设置显示精度
     *
     * @param precision Decimal places (e.g., 1 means display 25.5)
     *                  小数位数（如 1 表示显示 25.5）
     */
    void setPrecision(int precision);

    /**
     * Set icon | 设置图标
     *
     * @param icon Icon name (mdi format, e.g., "mdi:thermometer")
     *             图标名称（mdi 格式，如 "mdi:thermometer"）
     */
    void setIcon(const String& icon);

    // =========================================================================
    // Getters | 获取方法
    // =========================================================================

    const String& getId() const { return _id; }
    const String& getName() const { return _name; }
    float getValue() const { return _value; }
    const String& getDeviceClass() const { return _deviceClass; }
    const String& getUnit() const { return _unit; }
    const String& getStateClass() const { return _stateClass; }
    int getPrecision() const { return _precision; }
    const String& getIcon() const { return _icon; }

    // =========================================================================
    // Internal Methods | 内部方法
    // =========================================================================

    /**
     * Convert to JSON format (for sending to HA)
     * 转换为 JSON 格式（用于发送到 HA）
     */
    void toJson(JsonObject& obj) const;

private:
    String _id;           // Sensor ID | 传感器 ID
    String _name;         // Display name | 显示名称
    String _deviceClass;  // Device class | 设备类别
    String _unit;         // Unit | 单位
    String _stateClass;   // State class | 状态类别
    String _icon;         // Icon | 图标
    float _value;         // Current value | 当前值
    int _precision;       // Display precision | 显示精度
    bool _hasValue;       // Whether value is set | 是否已设置值

    // Associated main class instance | 关联的主类实例
    SeeedHADiscovery* _ha;
    friend class SeeedHADiscovery;

    // Notify value change | 通知值变化
    void _notifyChange();
};

// =============================================================================
// SeeedHASwitch - Switch Class | 开关类
// =============================================================================

/**
 * Switch class - represents a controllable switch device
 * 开关类 - 代表一个可控制的开关设备
 *
 * Used to receive control commands from Home Assistant.
 * 用于接收来自 Home Assistant 的控制命令，如控制 LED、继电器等。
 *
 * Workflow:
 * 工作流程：
 * 1. Create switch and register callback
 *    创建开关并注册回调函数
 * 2. Home Assistant sends switch command
 *    Home Assistant 发送开关命令
 * 3. Callback is invoked to perform actual hardware operation
 *    回调函数被调用，执行实际的硬件操作
 * 4. State is automatically synced back to Home Assistant
 *    状态自动同步回 Home Assistant
 *
 * Example:
 * 示例：
 * ```cpp
 * SeeedHASwitch* ledSwitch = ha.addSwitch("led", "LED");
 * ledSwitch->onStateChange([](bool state) {
 *     digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
 * });
 * ```
 */
class SeeedHASwitch {
public:
    /**
     * Constructor | 构造函数
     *
     * @param id Switch ID (unique identifier, e.g., "led")
     *           开关 ID（唯一标识符，如 "led"）
     * @param name Display name (e.g., "LED")
     *             显示名称（如 "LED灯"）
     * @param icon Icon (e.g., "mdi:lightbulb")
     *             图标（如 "mdi:lightbulb"）
     */
    SeeedHASwitch(
        const String& id,
        const String& name,
        const String& icon = ""
    );

    // =========================================================================
    // State Control | 状态控制
    // =========================================================================

    /**
     * Set switch state
     * 设置开关状态
     *
     * Calling this method updates state and syncs to Home Assistant.
     * 调用此方法会更新状态并同步到 Home Assistant。
     *
     * @param state Switch state (true = ON, false = OFF)
     *              开关状态（true = 开, false = 关）
     */
    void setState(bool state);

    /**
     * Toggle switch state
     * 切换开关状态
     *
     * If currently ON, turns OFF; if currently OFF, turns ON.
     * 如果当前是开，则关；如果当前是关，则开。
     */
    void toggle();

    // =========================================================================
    // Callback Registration | 回调注册
    // =========================================================================

    /**
     * Register state change callback
     * 注册状态变化回调函数
     *
     * This callback is invoked when Home Assistant sends switch command.
     * You should perform actual hardware operation in the callback.
     * 当 Home Assistant 发送开关命令时，此回调会被调用。
     * 你应该在回调中执行实际的硬件操作（如 digitalWrite）。
     *
     * @param callback Callback function, receives new state as parameter
     *                 回调函数，接收新状态作为参数
     *
     * Example:
     * 示例：
     * ```cpp
     * ledSwitch->onStateChange([](bool state) {
     *     digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
     *     Serial.printf("LED: %s\n", state ? "ON" : "OFF");
     * });
     * ```
     */
    void onStateChange(SwitchCallback callback);

    // =========================================================================
    // Getters | 获取方法
    // =========================================================================

    const String& getId() const { return _id; }
    const String& getName() const { return _name; }
    bool getState() const { return _state; }
    const String& getIcon() const { return _icon; }

    // =========================================================================
    // Configuration | 配置方法
    // =========================================================================

    /**
     * Set icon | 设置图标
     *
     * @param icon Icon name (mdi format, e.g., "mdi:lightbulb")
     *             图标名称（mdi 格式，如 "mdi:lightbulb"）
     */
    void setIcon(const String& icon);

    // =========================================================================
    // Internal Methods | 内部方法
    // =========================================================================

    /**
     * Convert to JSON format (for sending to HA)
     * 转换为 JSON 格式（用于发送到 HA）
     */
    void toJson(JsonObject& obj) const;

    /**
     * Handle command from HA (internal use)
     * 处理来自 HA 的命令（内部使用）
     */
    void _handleCommand(bool state);

private:
    String _id;              // Switch ID | 开关 ID
    String _name;            // Display name | 显示名称
    String _icon;            // Icon | 图标
    bool _state;             // Current state | 当前状态
    SwitchCallback _callback; // State change callback | 状态变化回调

    // Associated main class instance | 关联的主类实例
    SeeedHADiscovery* _ha;
    friend class SeeedHADiscovery;

    // Notify state change | 通知状态变化
    void _notifyChange();
};

// =============================================================================
// SeeedHAState - HA Entity State Class | HA 实体状态类
// =============================================================================

/**
 * HA Entity State class - stores state of a subscribed Home Assistant entity
 * HA 实体状态类 - 存储订阅的 Home Assistant 实体的状态
 *
 * This class is used to receive and store states from Home Assistant entities.
 * 这个类用于接收和存储来自 Home Assistant 实体的状态。
 *
 * The states are pushed from HA when:
 * 状态会在以下情况下从 HA 推送：
 * - Device connects to HA (initial sync) | 设备连接到 HA 时（初始同步）
 * - Entity state changes in HA | HA 中实体状态变化时
 *
 * Usage:
 * 使用方法：
 * ```cpp
 * // In setup(), register callback for HA state changes
 * ha.onHAState([](const char* entityId, const char* state, JsonObject& attrs) {
 *     Serial.printf("HA state: %s = %s\n", entityId, state);
 * });
 *
 * // Or access stored states directly
 * SeeedHAState* tempState = ha.getHAState("sensor.living_room_temperature");
 * if (tempState && tempState->hasValue()) {
 *     float temp = tempState->getFloat();
 *     Serial.printf("Temperature: %.1f\n", temp);
 * }
 * ```
 */
class SeeedHAState {
public:
    /**
     * Constructor | 构造函数
     *
     * @param entityId HA entity ID (e.g., "sensor.living_room_temperature")
     *                 HA 实体 ID（如 "sensor.living_room_temperature"）
     */
    SeeedHAState(const String& entityId);

    // =========================================================================
    // Value Getters | 值获取方法
    // =========================================================================

    /**
     * Check if value has been received
     * 检查是否已接收到值
     *
     * @return true if value is available | 如果有值返回 true
     */
    bool hasValue() const { return _hasValue; }

    /**
     * Get state as string
     * 获取字符串形式的状态
     *
     * @return State value as string | 字符串形式的状态值
     */
    const String& getString() const { return _state; }

    /**
     * Get state as float
     * 获取浮点数形式的状态
     *
     * Useful for numeric sensors like temperature, humidity.
     * 适用于温度、湿度等数值型传感器。
     *
     * @return State value as float, 0 if not a number
     *         浮点数形式的状态值，如果不是数字则返回 0
     */
    float getFloat() const;

    /**
     * Get state as integer
     * 获取整数形式的状态
     *
     * @return State value as integer, 0 if not a number
     *         整数形式的状态值，如果不是数字则返回 0
     */
    int getInt() const;

    /**
     * Get state as boolean
     * 获取布尔形式的状态
     *
     * Returns true for "on", "true", "1", "home", etc.
     * 对于 "on", "true", "1", "home" 等返回 true。
     *
     * @return State value as boolean | 布尔形式的状态值
     */
    bool getBool() const;

    // =========================================================================
    // Attribute Getters | 属性获取方法
    // =========================================================================

    /**
     * Get entity ID
     * 获取实体 ID
     */
    const String& getEntityId() const { return _entityId; }

    /**
     * Get friendly name
     * 获取友好名称
     */
    const String& getFriendlyName() const { return _friendlyName; }

    /**
     * Get unit of measurement
     * 获取单位
     */
    const String& getUnit() const { return _unit; }

    /**
     * Get device class
     * 获取设备类别
     */
    const String& getDeviceClass() const { return _deviceClass; }

    /**
     * Get last update timestamp (millis)
     * 获取最后更新时间戳（毫秒）
     */
    unsigned long getLastUpdate() const { return _lastUpdate; }

    // =========================================================================
    // Internal Methods | 内部方法
    // =========================================================================

    /**
     * Update state from HA (internal use)
     * 从 HA 更新状态（内部使用）
     */
    void _updateState(const String& state, JsonObject& attributes);

private:
    String _entityId;       // HA entity ID | HA 实体 ID
    String _state;          // Current state | 当前状态
    String _friendlyName;   // Friendly name | 友好名称
    String _unit;           // Unit of measurement | 单位
    String _deviceClass;    // Device class | 设备类别
    bool _hasValue;         // Whether value is set | 是否已设置值
    unsigned long _lastUpdate; // Last update time | 最后更新时间
};

// =============================================================================
// SeeedHADiscovery - Main Class | 主类
// =============================================================================

/**
 * Seeed Home Assistant Discovery Main Class
 * Seeed Home Assistant Discovery 主类
 *
 * This is the core class of the library, responsible for:
 * 这是库的核心类，负责：
 * - WiFi connection management | WiFi 连接管理
 * - mDNS service publishing | mDNS 服务发布
 * - HTTP server (device info) | HTTP 服务器（设备信息）
 * - WebSocket server (real-time communication) | WebSocket 服务器（实时通信）
 * - Sensor management | 传感器管理
 */
class SeeedHADiscovery {
public:
    /**
     * Constructor | 构造函数
     */
    SeeedHADiscovery();

    /**
     * Destructor | 析构函数
     */
    ~SeeedHADiscovery();

    // =========================================================================
    // Configuration Methods | 配置方法
    // =========================================================================

    /**
     * Set device information
     * 设置设备信息
     *
     * This information is displayed on Home Assistant device page.
     * 这些信息会显示在 Home Assistant 的设备页面。
     *
     * @param name Device name (e.g., "Living Room Sensor")
     *             设备名称（如 "客厅温湿度传感器"）
     * @param model Device model (e.g., "ESP32-C3")
     *              设备型号（如 "ESP32-C3"）
     * @param version Firmware version (e.g., "1.0.0")
     *                固件版本（如 "1.0.0"）
     */
    void setDeviceInfo(const String& name, const String& model, const String& version = SEEED_HA_DISCOVERY_VERSION);

    /**
     * Enable debug output
     * 启用调试输出
     *
     * When enabled, debug info is output via Serial.
     * 启用后，会通过 Serial 输出调试信息。
     *
     * @param enable Whether to enable debug (default true)
     *               是否启用调试（默认 true）
     */
    void enableDebug(bool enable = true);

    // =========================================================================
    // Connection Methods | 连接方法
    // =========================================================================

    /**
     * Connect to WiFi and start services
     * 连接 WiFi 并启动服务
     *
     * This method will:
     * 这个方法会：
     * 1. Connect to specified WiFi network | 连接到指定的 WiFi 网络
     * 2. Start mDNS service (for device discovery) | 启动 mDNS 服务（用于设备自动发现）
     * 3. Start HTTP server (for device info) | 启动 HTTP 服务器（提供设备信息接口）
     * 4. Start WebSocket server (for real-time communication) | 启动 WebSocket 服务器（用于实时通信）
     *
     * @param ssid WiFi SSID | WiFi 名称
     * @param password WiFi password | WiFi 密码
     * @return Whether connection succeeded | 连接是否成功
     */
    bool begin(const char* ssid, const char* password);

    /**
     * Begin with WiFi provisioning support
     * 使用 WiFi 配网支持启动
     *
     * This method provides WiFi configuration:
     * 这个方法提供 WiFi 配置：
     * 1. Try to connect using saved credentials (if any)
     *    尝试使用保存的凭据连接（如果有）
     * 2. If no credentials or connection fails, start AP mode
     *    如果没有凭据或连接失败，启动 AP 模式
     * 3. Users can configure WiFi via captive portal
     *    用户可以通过强制门户配置 WiFi
     * 4. Credentials are saved and used on next boot
     *    凭据被保存并在下次启动时使用
     *
     * @param apSSID AP hotspot name (default: "Seeed_IoT_Device_AP")
     *               AP 热点名称（默认："Seeed_IoT_Device_AP"）
     * @return true if WiFi connected, false if AP mode is active (call handle() in loop)
     *         WiFi 已连接返回 true，AP 模式激活返回 false（在 loop 中调用 handle()）
     *
     * Example:
     * 示例：
     * ```cpp
     * SeeedHADiscovery ha;
     * bool wifiConnected = false;
     *
     * void setup() {
     *     wifiConnected = ha.beginWithProvisioning("My_IoT_Device_AP");
     * }
     *
     * void loop() {
     *     ha.handle();  // Must call! Handles both HA and provisioning
     *                   // 必须调用！处理 HA 和配网
     * }
     * ```
     */
    bool beginWithProvisioning(const String& apSSID = "Seeed_IoT_Device_AP");

    /**
     * Check if WiFi provisioning (AP mode) is active
     * 检查 WiFi 配网（AP 模式）是否激活
     *
     * @return true if AP mode is active for provisioning
     *         如果 AP 模式正在配网则返回 true
     */
    bool isProvisioningActive() const;

    /**
     * Get WiFi provisioning instance
     * 获取 WiFi 配网实例
     *
     * @return Pointer to SeeedWiFiProvisioning instance, or nullptr if not using provisioning
     *         SeeedWiFiProvisioning 实例指针，如果未使用配网则返回 nullptr
     */
    SeeedWiFiProvisioning* getProvisioning() { return _provisioning; }

    /**
     * Clear saved WiFi credentials
     * 清除保存的 WiFi 凭据
     *
     * This will clear saved credentials from flash.
     * On next boot, device will enter AP mode for reconfiguration.
     * 这会从 Flash 中清除保存的凭据。
     * 下次启动时，设备将进入 AP 模式重新配置。
     */
    void clearWiFiCredentials();

    /**
     * Enable reset button for WiFi re-provisioning
     * 启用重置按钮用于重新配网
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
    // Sensor Management | 传感器管理
    // =========================================================================

    /**
     * Add a sensor
     * 添加一个传感器
     *
     * Creates a new sensor entity, automatically registered to Home Assistant.
     * 创建一个新的传感器实体，会自动注册到 Home Assistant。
     *
     * @param id Sensor ID (unique identifier, e.g., "temperature")
     *           传感器 ID（唯一标识符，如 "temperature"）
     * @param name Display name (e.g., "Temperature")
     *             显示名称（如 "温度"）
     * @param deviceClass Device class (e.g., "temperature", "humidity")
     *                    设备类别（如 "temperature", "humidity"）
     *                    Ref: https://www.home-assistant.io/integrations/sensor/#device-class
     * @param unit Unit (e.g., "°C", "%")
     *             单位（如 "°C", "%"）
     * @return Sensor object pointer | 传感器对象指针
     *
     * Common device classes:
     * 常用设备类别：
     * - temperature: Temperature | 温度
     * - humidity: Humidity | 湿度
     * - pressure: Pressure | 气压
     * - illuminance: Illuminance | 光照
     * - battery: Battery level | 电池电量
     * - voltage: Voltage | 电压
     * - current: Current | 电流
     * - power: Power | 功率
     */
    SeeedHASensor* addSensor(
        const String& id,
        const String& name,
        const String& deviceClass = "",
        const String& unit = ""
    );

    // =========================================================================
    // Switch Management | 开关管理
    // =========================================================================

    /**
     * Add a switch
     * 添加一个开关
     *
     * Creates a new switch entity for receiving Home Assistant control commands.
     * 创建一个新的开关实体，用于接收 Home Assistant 的控制命令。
     *
     * @param id Switch ID (unique identifier, e.g., "led")
     *           开关 ID（唯一标识符，如 "led"）
     * @param name Display name (e.g., "LED")
     *             显示名称（如 "LED灯"）
     * @param icon Icon (e.g., "mdi:lightbulb")
     *             图标（如 "mdi:lightbulb"）
     * @return Switch object pointer | 开关对象指针
     *
     * Example:
     * 示例：
     * ```cpp
     * SeeedHASwitch* led = ha.addSwitch("led", "LED", "mdi:led-on");
     * led->onStateChange([](bool state) {
     *     digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
     * });
     * ```
     */
    SeeedHASwitch* addSwitch(
        const String& id,
        const String& name,
        const String& icon = ""
    );

    // =========================================================================
    // HA State Subscription | HA 状态订阅
    // =========================================================================

    /**
     * Register callback for HA state changes
     * 注册 HA 状态变化回调
     *
     * This callback is invoked when Home Assistant pushes entity state updates.
     * 当 Home Assistant 推送实体状态更新时，此回调会被调用。
     *
     * Note: You need to configure which entities to subscribe in HA integration settings.
     * 注意：你需要在 HA 集成设置中配置要订阅哪些实体。
     *
     * @param callback Callback function receiving entity ID, state, and attributes
     *                 回调函数，接收实体 ID、状态和属性
     *
     * Example:
     * 示例：
     * ```cpp
     * ha.onHAState([](const char* entityId, const char* state, JsonObject& attrs) {
     *     Serial.printf("HA state: %s = %s\n", entityId, state);
     *     String unit = attrs["unit_of_measurement"] | "";
     *     Serial.printf("Unit: %s\n", unit.c_str());
     * });
     * ```
     */
    void onHAState(HAStateCallback callback);

    /**
     * Get HA entity state by entity ID
     * 通过实体 ID 获取 HA 实体状态
     *
     * Returns the stored state for a subscribed HA entity.
     * 返回订阅的 HA 实体的存储状态。
     *
     * @param entityId HA entity ID (e.g., "sensor.living_room_temperature")
     *                 HA 实体 ID（如 "sensor.living_room_temperature"）
     * @return Pointer to SeeedHAState object, or nullptr if not found
     *         SeeedHAState 对象指针，如果未找到则返回 nullptr
     *
     * Example:
     * 示例：
     * ```cpp
     * SeeedHAState* temp = ha.getHAState("sensor.living_room_temperature");
     * if (temp && temp->hasValue()) {
     *     float value = temp->getFloat();
     *     String unit = temp->getUnit();
     *     Serial.printf("Temperature: %.1f %s\n", value, unit.c_str());
     * }
     * ```
     */
    SeeedHAState* getHAState(const String& entityId);

    /**
     * Get all subscribed HA states
     * 获取所有订阅的 HA 状态
     *
     * @return Reference to the map of entity ID to SeeedHAState pointers
     *         实体 ID 到 SeeedHAState 指针的映射引用
     */
    const std::map<String, SeeedHAState*>& getHAStates() const { return _haStates; }

    /**
     * Clear all subscribed HA states
     * 清除所有订阅的 HA 状态
     *
     * Called internally when subscription is updated from HA.
     * 当 HA 更新订阅时内部调用。
     */
    void clearHAStates();

    // =========================================================================
    // Runtime Methods | 运行时方法
    // =========================================================================

    /**
     * Handle network events
     * 处理网络事件
     *
     * Must be called in loop()!
     * 必须在 loop() 中调用！
     *
     * This method handles:
     * 这个方法处理：
     * - HTTP requests | HTTP 请求
     * - WebSocket messages | WebSocket 消息
     * - Heartbeat detection | 心跳检测
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
     * Check if Home Assistant is connected
     * 检查是否有 Home Assistant 连接
     */
    bool isHAConnected() const;

    /**
     * Notify Home Assistant that device is about to enter sleep mode
     * 通知 Home Assistant 设备即将进入休眠模式
     *
     * Call this before entering deep sleep so HA can start reconnecting immediately.
     * 在进入深度睡眠前调用此方法，让 HA 可以立即开始重连。
     *
     * This sends a "sleep" message to HA and closes the WebSocket connection.
     * 这会发送一个 "sleep" 消息到 HA 并关闭 WebSocket 连接。
     */
    void notifySleep();

    /**
     * Get local IP address
     * 获取本机 IP 地址
     */
    IPAddress getLocalIP() const;

    /**
     * Get device ID
     * 获取设备 ID
     */
    const String& getDeviceId() const { return _deviceId; }

    // =========================================================================
    // Internal Methods (used by entity classes)
    // 内部方法（实体类使用）
    // =========================================================================

    void _notifySensorChange(const String& sensorId);
    void _notifySwitchChange(const String& switchId);

private:
    // -------------------------------------------------------------------------
    // Device Information | 设备信息
    // -------------------------------------------------------------------------
    String _deviceName;    // Device name | 设备名称
    String _deviceModel;   // Device model | 设备型号
    String _deviceVersion; // Firmware version | 固件版本
    String _deviceId;      // Device unique ID (based on MAC) | 设备唯一 ID（基于 MAC 地址）

    // -------------------------------------------------------------------------
    // Network Services | 网络服务
    // -------------------------------------------------------------------------
    WebServer* _httpServer;         // HTTP server | HTTP 服务器
    WebSocketsServer* _wsServer;    // WebSocket server | WebSocket 服务器
    bool _wsClientConnected;        // Whether WebSocket client connected | 是否有 WebSocket 客户端连接

    // -------------------------------------------------------------------------
    // WiFi Provisioning | WiFi 配网
    // -------------------------------------------------------------------------
    SeeedWiFiProvisioning* _provisioning;  // WiFi provisioning instance | WiFi 配网实例

    // -------------------------------------------------------------------------
    // Sensor List | 传感器列表
    // -------------------------------------------------------------------------
    std::vector<SeeedHASensor*> _sensors;

    // -------------------------------------------------------------------------
    // Switch List | 开关列表
    // -------------------------------------------------------------------------
    std::vector<SeeedHASwitch*> _switches;

    // -------------------------------------------------------------------------
    // HA State Subscription | HA 状态订阅
    // -------------------------------------------------------------------------
    std::map<String, SeeedHAState*> _haStates;  // Subscribed HA states | 订阅的 HA 状态
    HAStateCallback _haStateCallback;            // HA state change callback | HA 状态变化回调

    // -------------------------------------------------------------------------
    // State Variables | 状态变量
    // -------------------------------------------------------------------------
    bool _debug;                  // Debug mode | 调试模式
    unsigned long _lastHeartbeat; // Last heartbeat time | 上次心跳时间

    // -------------------------------------------------------------------------
    // Internal Methods | 内部方法
    // -------------------------------------------------------------------------

    // Generate device ID (based on MAC) | 生成设备 ID（基于 MAC 地址）
    String _generateDeviceId();

    // Setup mDNS service | 设置 mDNS 服务
    void _setupMDNS();

    // Setup HTTP server | 设置 HTTP 服务器
    void _setupHTTP();

    // Setup WebSocket server | 设置 WebSocket 服务器
    void _setupWebSocket();

    // HTTP request handlers | HTTP 请求处理
    void _handleHTTPRoot();     // Home page | 主页
    void _handleHTTPInfo();     // Device info API | 设备信息接口

    // WebSocket event handler | WebSocket 事件处理
    void _handleWSEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

    // Send discovery info (list of supported sensors)
    // 发送发现信息（设备支持的传感器列表）
    void _sendDiscovery(uint8_t clientNum = 255);

    // Send sensor state update | 发送传感器状态更新
    void _sendSensorState(const String& sensorId, uint8_t clientNum = 255);

    // Send switch state update | 发送开关状态更新
    void _sendSwitchState(const String& switchId, uint8_t clientNum = 255);

    // Handle command from HA | 处理来自 HA 的命令消息
    void _handleCommand(JsonDocument& doc);

    // Handle HA state push | 处理来自 HA 的状态推送
    void _handleHAState(JsonDocument& doc);

    // Broadcast message to all WebSocket clients
    // 广播消息到所有 WebSocket 客户端
    void _broadcastMessage(const String& message);

    // Debug log output | 调试日志输出
    void _log(const String& message);
};

#endif // SEEED_HA_DISCOVERY_H
