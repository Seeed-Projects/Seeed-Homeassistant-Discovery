/**
 * ============================================================================
 * Seeed Home Assistant Discovery BLE - Bluetooth Arduino Library
 * Seeed Home Assistant Discovery BLE - 蓝牙版 Arduino 库
 * ============================================================================
 *
 * This library enables ESP32 and nRF52840 devices to connect to Home Assistant via Bluetooth:
 * 这个库让 ESP32 和 nRF52840 设备能够通过蓝牙连接到 Home Assistant：
 * - Supports XIAO nRF52840, XIAO ESP32-C3/C6/S3
 *   支持 XIAO nRF52840、XIAO ESP32-C3/C6/S3
 * - Based on BTHome v2 protocol, native HA support
 *   基于 BTHome v2 协议，HA 原生支持
 * - Supports sensor data reporting (passive broadcast)
 *   支持传感器数据上报（被动广播）
 * - Supports switch control (GATT bidirectional communication)
 *   支持开关控制（GATT 双向通信）
 *
 * Usage:
 * 使用方法：
 * 1. Create SeeedHADiscoveryBLE instance
 *    创建 SeeedHADiscoveryBLE 实例
 * 2. Call begin() to initialize BLE
 *    调用 begin() 初始化 BLE
 * 3. Use addSensor() to add sensors / addSwitch() to add switches
 *    使用 addSensor() 添加传感器 / addSwitch() 添加开关
 * 4. Call loop() periodically to handle events
 *    定期调用 loop() 处理事件
 *
 * @author limengdu
 * @version 1.5.0
 * @license MIT
 */

#ifndef SEEED_HA_DISCOVERY_BLE_H
#define SEEED_HA_DISCOVERY_BLE_H

#include <Arduino.h>
#include <functional>

// =============================================================================
// Platform Detection | 平台检测
// =============================================================================

#if defined(ESP32)
    #include <NimBLEDevice.h>
    #define SEEED_BLE_ESP32
    #define SEEED_BLE_PLATFORM "ESP32 (NimBLE)"

#elif defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_NRF52840)
    #include <ArduinoBLE.h>
    #define SEEED_BLE_MBED_NRF52840
    #define SEEED_BLE_PLATFORM "nRF52840 (ArduinoBLE)"

#elif defined(ARDUINO_NRF52_ADAFRUIT) || defined(ARDUINO_ARCH_NRF52)
    #include <bluefruit.h>
    #define SEEED_BLE_NRF52_BLUEFRUIT
    #define SEEED_BLE_PLATFORM "nRF52 (Bluefruit)"

#else
    #error "Unsupported platform. Supported: XIAO ESP32 series, XIAO nRF52840 (mbed or Adafruit BSP)"
#endif

#include <vector>

// =============================================================================
// Version and Constants | 版本和常量
// =============================================================================

#define SEEED_BLE_VERSION "1.6.0"

// Seeed Manufacturer ID (0x5EED = "SEED")
#define SEEED_MANUFACTURER_ID 0x5EED

// BTHome Service UUID
#define BTHOME_SERVICE_UUID 0xFCD2
#define BTHOME_SERVICE_UUID_STR "0000fcd2-0000-1000-8000-00805f9b34fb"

// Seeed HA Control Service UUID (custom)
// Used for bidirectional control communication
// Seeed HA 控制服务 UUID（自定义）
// 用于双向通信控制
#define SEEED_CONTROL_SERVICE_UUID        "5eed0001-b5a3-f393-e0a9-e50e24dcca9e"
#define SEEED_CONTROL_COMMAND_CHAR_UUID   "5eed0002-b5a3-f393-e0a9-e50e24dcca9e"
#define SEEED_CONTROL_STATE_CHAR_UUID     "5eed0003-b5a3-f393-e0a9-e50e24dcca9e"
#define SEEED_HA_STATE_CHAR_UUID          "5eed0004-b5a3-f393-e0a9-e50e24dcca9e"

// Maximum subscribed HA entities | 最大订阅 HA 实体数量
#define SEEED_BLE_MAX_HA_ENTITIES 16

// BTHome Device Info Flags | BTHome 设备信息标志
#define BTHOME_DEVICE_INFO_ENCRYPT  0x01
#define BTHOME_DEVICE_INFO_TRIGGER  0x04
#define BTHOME_DEVICE_INFO_VERSION  0x40

// =============================================================================
// BTHome Sensor Type Definitions (BTHome v2 Spec)
// BTHome 传感器类型定义 (BTHome v2 规范)
// =============================================================================

enum BTHomeObjectId : uint8_t {
    BTHOME_PACKET_ID        = 0x00,
    BTHOME_BATTERY          = 0x01,
    BTHOME_TEMPERATURE      = 0x02,
    BTHOME_HUMIDITY         = 0x03,
    BTHOME_PRESSURE         = 0x04,
    BTHOME_ILLUMINANCE      = 0x05,
    BTHOME_MASS_KG          = 0x06,
    BTHOME_MASS_LB          = 0x07,
    BTHOME_DEWPOINT         = 0x08,
    BTHOME_COUNT_UINT8      = 0x09,
    BTHOME_ENERGY           = 0x0A,
    BTHOME_POWER            = 0x0B,
    BTHOME_VOLTAGE          = 0x0C,
    BTHOME_PM25             = 0x0D,
    BTHOME_PM10             = 0x0E,
    BTHOME_BINARY_GENERIC   = 0x0F,
    BTHOME_BINARY_POWER     = 0x10,
    BTHOME_BINARY_OPENING   = 0x11,
    BTHOME_CO2              = 0x12,
    BTHOME_TVOC             = 0x13,
    BTHOME_MOISTURE         = 0x14,
    BTHOME_BINARY_BATTERY_LOW = 0x15,
    BTHOME_BINARY_BATTERY_CHARGING = 0x16,
    BTHOME_COUNT_UINT16     = 0x3D,
    BTHOME_COUNT_UINT32     = 0x3E,
    BTHOME_ROTATION         = 0x3F,
    BTHOME_DISTANCE_MM      = 0x40,
    BTHOME_DISTANCE_M       = 0x41,
    BTHOME_DURATION         = 0x42,
    BTHOME_CURRENT          = 0x43,
    BTHOME_SPEED            = 0x44,
    BTHOME_TEMPERATURE_TENTH = 0x45,
    BTHOME_UV_INDEX         = 0x46,
    BTHOME_VOLUME_LITERS    = 0x47,
    BTHOME_VOLUME_ML        = 0x48,
    BTHOME_VOLUME_FLOW      = 0x49,
    BTHOME_VOLTAGE_TENTH    = 0x4A,
    BTHOME_GAS              = 0x4B,
    BTHOME_GAS_UINT32       = 0x4C,
    BTHOME_ENERGY_UINT32    = 0x4D,
    BTHOME_VOLUME_UINT32    = 0x4E,
    BTHOME_WATER            = 0x4F,
    BTHOME_HUMIDITY_UINT8   = 0x2E,
    BTHOME_MOISTURE_UINT8   = 0x2F,
    BTHOME_BINARY_OCCUPANCY = 0x20,
    BTHOME_BINARY_MOTION    = 0x21,
    BTHOME_BUTTON           = 0x3A,
};

// Button Event Types | 按钮事件类型
enum BTHomeButtonEvent : uint8_t {
    BTHOME_BUTTON_NONE       = 0x00,
    BTHOME_BUTTON_PRESS      = 0x01,
    BTHOME_BUTTON_DOUBLE     = 0x02,
    BTHOME_BUTTON_TRIPLE     = 0x03,
    BTHOME_BUTTON_LONG_PRESS = 0x04,
    BTHOME_BUTTON_LONG_DOUBLE = 0x05,
    BTHOME_BUTTON_LONG_TRIPLE = 0x06,
};

// =============================================================================
// Forward Declarations | 前向声明
// =============================================================================

class SeeedHADiscoveryBLE;
class SeeedBLESensor;
class SeeedBLESwitch;

// =============================================================================
// Callback Types | 回调函数类型
// =============================================================================

// Switch state change callback | 开关状态变化回调
typedef std::function<void(bool state)> BLESwitchCallback;

// HA state change callback | HA 状态变化回调
// Called when Home Assistant pushes entity state to device
// 当 Home Assistant 推送实体状态到设备时调用
typedef std::function<void(uint8_t entityIndex, const char* entityId, const char* state, float numericValue)> BLEHAStateCallback;

// =============================================================================
// SeeedBLESensor - BLE Sensor Class | BLE 传感器类
// =============================================================================

class SeeedBLESensor {
public:
    SeeedBLESensor(BTHomeObjectId objectId);

    void setValue(int32_t value);
    void setValue(float value);
    void setState(bool state);
    void triggerButton(BTHomeButtonEvent event);

    BTHomeObjectId getObjectId() const { return _objectId; }
    int32_t getRawValue() const { return _rawValue; }
    bool hasValue() const { return _hasValue; }

    uint8_t getDataSize() const;
    void writeToBuffer(uint8_t* buffer, uint8_t& offset) const;

private:
    BTHomeObjectId _objectId;
    int32_t _rawValue;
    bool _hasValue;
    float _getMultiplier() const;
};

// =============================================================================
// SeeedBLEHAState - BLE HA Entity State Class | BLE HA 实体状态类
// =============================================================================

/**
 * HA Entity State class - stores state of a subscribed Home Assistant entity
 * HA 实体状态类 - 存储订阅的 Home Assistant 实体的状态
 *
 * This class receives and stores states pushed from Home Assistant via BLE.
 * 这个类接收和存储通过 BLE 从 Home Assistant 推送的状态。
 *
 * Usage | 使用方法:
 * ```cpp
 * // Subscribe to entities
 * ble.subscribeEntity(0, "sensor.temperature");
 * ble.subscribeEntity(1, "switch.light");
 *
 * // Register callback
 * ble.onHAState([](uint8_t index, const char* entityId, const char* state, float value) {
 *     Serial.printf("Entity %s = %s (%.2f)\n", entityId, state, value);
 * });
 *
 * // Or access states directly
 * SeeedBLEHAState* temp = ble.getHAState(0);
 * if (temp && temp->hasValue()) {
 *     Serial.printf("Temperature: %.1f\n", temp->getFloat());
 * }
 * ```
 */
class SeeedBLEHAState {
public:
    /**
     * Constructor | 构造函数
     * @param entityIndex Index of the entity | 实体索引
     */
    SeeedBLEHAState(uint8_t entityIndex);

    // =========================================================================
    // Value Getters | 值获取方法
    // =========================================================================

    /**
     * Check if value has been received
     * 检查是否已接收到值
     */
    bool hasValue() const { return _hasValue; }

    /**
     * Get state as string | 获取字符串形式的状态
     */
    const char* getString() const { return _state; }

    /**
     * Get state as float | 获取浮点数形式的状态
     * Useful for numeric sensors like temperature, humidity.
     */
    float getFloat() const { return _numericValue; }

    /**
     * Get state as integer | 获取整数形式的状态
     */
    int getInt() const { return (int)_numericValue; }

    /**
     * Get state as boolean | 获取布尔形式的状态
     * Returns true for "on", "true", "1", "home", etc.
     */
    bool getBool() const;

    // =========================================================================
    // Attribute Getters | 属性获取方法
    // =========================================================================

    /**
     * Get entity index | 获取实体索引
     */
    uint8_t getEntityIndex() const { return _entityIndex; }

    /**
     * Get entity ID | 获取实体 ID
     */
    const char* getEntityId() const { return _entityId; }

    /**
     * Get last update timestamp (millis) | 获取最后更新时间戳
     */
    unsigned long getLastUpdate() const { return _lastUpdate; }

    // =========================================================================
    // Internal Methods | 内部方法
    // =========================================================================

    /**
     * Set entity ID | 设置实体 ID
     */
    void setEntityId(const char* entityId);

    /**
     * Update state from HA (internal use)
     * 从 HA 更新状态（内部使用）
     */
    void _updateState(const char* state, float numericValue);

private:
    uint8_t _entityIndex;       // Entity index | 实体索引
    char _entityId[64];         // HA entity ID | HA 实体 ID
    char _state[32];            // Current state string | 当前状态字符串
    float _numericValue;        // Numeric value | 数值
    bool _hasValue;             // Whether value is set | 是否已设置值
    unsigned long _lastUpdate;  // Last update time | 最后更新时间
};

// =============================================================================
// SeeedBLESwitch - BLE Switch Class | BLE 开关类
// =============================================================================

/**
 * BLE Switch Class - supports receiving control commands from Home Assistant
 * BLE 开关类 - 支持从 Home Assistant 接收控制命令
 * 
 * Uses GATT characteristics for bidirectional communication:
 * 使用 GATT 特征值实现双向通信：
 * - HA writes to command characteristic to control switch
 *   HA 写入命令特征值来控制开关
 * - Device notifies HA of current state via state characteristic
 *   设备通过状态特征值通知 HA 当前状态
 */
class SeeedBLESwitch {
public:
    /**
     * Constructor | 构造函数
     * @param id Switch ID (for identification) | 开关 ID（用于识别）
     * @param name Switch name (for display) | 开关名称（显示用）
     */
    SeeedBLESwitch(const char* id, const char* name);

    /**
     * Set switch state | 设置开关状态
     * @param state true = ON, false = OFF
     */
    void setState(bool state);

    /**
     * Toggle switch state | 切换开关状态
     */
    void toggle();

    /**
     * Get current state | 获取当前状态
     */
    bool getState() const { return _state; }

    /**
     * Get switch ID | 获取开关 ID
     */
    const char* getId() const { return _id; }

    /**
     * Get switch name | 获取开关名称
     */
    const char* getName() const { return _name; }

    /**
     * Register state change callback
     * Called when HA sends control command
     * 注册状态变化回调
     * 当 HA 发送控制命令时调用
     */
    void onStateChange(BLESwitchCallback callback) { _callback = callback; }

    // Internal use | 内部使用
    void _handleCommand(bool state);
    void _setParent(SeeedHADiscoveryBLE* parent) { _parent = parent; }

private:
    char _id[32];
    char _name[32];
    bool _state;
    BLESwitchCallback _callback;
    SeeedHADiscoveryBLE* _parent;
};

// =============================================================================
// SeeedHADiscoveryBLE - Main Class | 主类
// =============================================================================

class SeeedHADiscoveryBLE {
public:
    SeeedHADiscoveryBLE();
    ~SeeedHADiscoveryBLE();

    // =========================================================================
    // Configuration Methods | 配置方法
    // =========================================================================

    void setDeviceName(const char* name);
    void enableDebug(bool enable = true);
    void setAdvertiseInterval(uint32_t intervalMs);
    void setTxPower(int8_t power);

    // =========================================================================
    // Initialization | 初始化
    // =========================================================================

    /**
     * Initialize BLE (sensor only mode, passive broadcast)
     * 初始化 BLE（仅传感器模式，被动广播）
     */
    bool begin(const char* deviceName = "Seeed Sensor");

    /**
     * Initialize BLE (bidirectional mode, supports control)
     * 初始化 BLE（双向通信模式，支持控制）
     * @param deviceName Device name | 设备名称
     * @param enableControl Whether to enable control (GATT service) | 是否启用控制功能（GATT 服务）
     */
    bool begin(const char* deviceName, bool enableControl);

    /**
     * Stop BLE | 停止 BLE
     */
    void stop();

    /**
     * Handle BLE events (must call in loop)
     * Used for handling GATT connections and commands
     * 处理 BLE 事件（必须在 loop 中调用）
     * 用于处理 GATT 连接和命令
     */
    void loop();

    // =========================================================================
    // Sensor Management | 传感器管理
    // =========================================================================

    SeeedBLESensor* addSensor(BTHomeObjectId objectId);
    SeeedBLESensor* addTemperature() { return addSensor(BTHOME_TEMPERATURE); }
    SeeedBLESensor* addHumidity() { return addSensor(BTHOME_HUMIDITY); }
    SeeedBLESensor* addBattery() { return addSensor(BTHOME_BATTERY); }
    SeeedBLESensor* addButton() { return addSensor(BTHOME_BUTTON); }

    // =========================================================================
    // Switch Management | 开关管理
    // =========================================================================

    /**
     * Add switch | 添加开关
     * @param id Switch ID | 开关 ID
     * @param name Switch name | 开关名称
     * @return Switch object pointer | 开关对象指针
     */
    SeeedBLESwitch* addSwitch(const char* id, const char* name);

    // =========================================================================
    // HA State Subscription | HA 状态订阅
    // =========================================================================

    /**
     * Subscribe to a Home Assistant entity
     * 订阅 Home Assistant 实体
     *
     * Call this in setup() before begin() to specify which HA entities to receive.
     * 在 setup() 中、begin() 之前调用，指定要接收哪些 HA 实体。
     *
     * @param entityIndex Index for this entity (0 to SEEED_BLE_MAX_HA_ENTITIES-1)
     *                    此实体的索引（0 到 SEEED_BLE_MAX_HA_ENTITIES-1）
     * @param entityId HA entity ID (e.g., "sensor.temperature")
     *                 HA 实体 ID（如 "sensor.temperature"）
     * @return Pointer to SeeedBLEHAState object | SeeedBLEHAState 对象指针
     *
     * Example | 示例:
     * ```cpp
     * ble.subscribeEntity(0, "sensor.living_room_temperature");
     * ble.subscribeEntity(1, "sensor.humidity");
     * ble.subscribeEntity(2, "switch.light");
     * ```
     */
    SeeedBLEHAState* subscribeEntity(uint8_t entityIndex, const char* entityId);

    /**
     * Register callback for HA state changes
     * 注册 HA 状态变化回调
     *
     * This callback is invoked when Home Assistant pushes entity state updates.
     * 当 Home Assistant 推送实体状态更新时，此回调会被调用。
     *
     * @param callback Callback function receiving entity index, ID, state, and numeric value
     *                 回调函数，接收实体索引、ID、状态和数值
     *
     * Example | 示例:
     * ```cpp
     * ble.onHAState([](uint8_t index, const char* entityId, const char* state, float value) {
     *     Serial.printf("HA[%d] %s = %s", index, entityId, state);
     *     if (value != 0) Serial.printf(" (%.2f)", value);
     *     Serial.println();
     * });
     * ```
     */
    void onHAState(BLEHAStateCallback callback);

    /**
     * Get HA entity state by index
     * 通过索引获取 HA 实体状态
     *
     * @param entityIndex Index of the subscribed entity
     *                    订阅实体的索引
     * @return Pointer to SeeedBLEHAState object, or nullptr if not found
     *         SeeedBLEHAState 对象指针，如果未找到则返回 nullptr
     */
    SeeedBLEHAState* getHAState(uint8_t entityIndex);

    /**
     * Get subscribed entity count
     * 获取订阅的实体数量
     */
    uint8_t getSubscribedEntityCount() const { return _haStateCount; }

    // =========================================================================
    // Broadcasting | 广播
    // =========================================================================

    void advertise();
    void updateAdvertiseData();

    // =========================================================================
    // Status Queries | 状态查询
    // =========================================================================

    bool isRunning() const { return _running; }
    bool isConnected() const { return _connected; }
    const char* getDeviceName() const { return _deviceName; }
    String getAddress();

    // =========================================================================
    // Internal Methods (for callbacks) | 内部方法（供回调使用）
    // =========================================================================

    void _handleCommand(const uint8_t* data, size_t length);
    void _handleHAState(const uint8_t* data, size_t length);
    void _notifyStateChange();
    void _sendSubscriptionInfo();
    void _onConnect();
    void _onDisconnect();

private:
    // Device configuration | 设备配置
    char _deviceName[21];
    bool _debug;
    bool _running;
    bool _connected;
    bool _controlEnabled;
    uint32_t _advertiseInterval;
    int8_t _txPower;
    uint8_t _packetId;

    // Sensor and switch lists | 传感器和开关列表
    std::vector<SeeedBLESensor*> _sensors;
    std::vector<SeeedBLESwitch*> _switches;

    // HA state subscription | HA 状态订阅
    SeeedBLEHAState* _haStates[SEEED_BLE_MAX_HA_ENTITIES];
    uint8_t _haStateCount;
    BLEHAStateCallback _haStateCallback;

    // Advertise data buffer | 广播数据缓冲区
    uint8_t _advData[31];
    uint8_t _advDataLen;

    // Platform-specific BLE objects | 平台特定的 BLE 对象
#ifdef SEEED_BLE_ESP32
    NimBLEServer* _pServer;
    NimBLEService* _pControlService;
    NimBLECharacteristic* _pCommandChar;
    NimBLECharacteristic* _pStateChar;
    NimBLECharacteristic* _pHAStateChar;
    NimBLEAdvertising* _pAdvertising;
#elif defined(SEEED_BLE_MBED_NRF52840)
    BLEService* _pControlService;
    BLECharacteristic* _pCommandChar;
    BLECharacteristic* _pStateChar;
    BLECharacteristic* _pHAStateChar;
#endif

    // Internal methods | 内部方法
    void _buildAdvData();
    void _setupGATTServer();
    void _buildStateData(uint8_t* buffer, size_t* length);
    void _log(const char* message);
    void _logf(const char* format, ...);
};

#endif // SEEED_HA_DISCOVERY_BLE_H
