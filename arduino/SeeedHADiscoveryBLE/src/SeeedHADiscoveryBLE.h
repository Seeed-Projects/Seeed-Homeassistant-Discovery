/**
 * ============================================================================
 * Seeed Home Assistant Discovery BLE - 蓝牙版 Arduino 库
 * Seeed Home Assistant Discovery BLE - Bluetooth Arduino Library
 * ============================================================================
 *
 * 这个库让 ESP32 和 nRF52840 设备能够通过蓝牙连接到 Home Assistant：
 * - 支持 XIAO nRF52840、XIAO ESP32-C3/C6/S3
 * - 基于 BTHome v2 协议，HA 原生支持
 * - 被动广播模式，超低功耗
 * - 简单易用的 API
 *
 * 使用方法：
 * 1. 创建 SeeedHADiscoveryBLE 实例
 * 2. 调用 begin() 初始化 BLE
 * 3. 使用 addSensor() 添加传感器
 * 4. 定期调用 advertise() 发送广播
 *
 * 示例：
 * ```cpp
 * SeeedHADiscoveryBLE ble;
 * SeeedBLESensor* temp;
 *
 * void setup() {
 *     ble.begin("温度传感器");
 *     temp = ble.addSensor(BTHOME_TEMPERATURE);
 * }
 *
 * void loop() {
 *     temp->setValue(25.5);
 *     ble.advertise();
 *     delay(5000);
 * }
 * ```
 *
 * @author limengdu
 * @version 1.0.0
 * @license MIT
 */

#ifndef SEEED_HA_DISCOVERY_BLE_H
#define SEEED_HA_DISCOVERY_BLE_H

#include <Arduino.h>

// 根据平台选择 BLE 库
#if defined(ARDUINO_ARCH_NRF52)
    // nRF52840 使用 Bluefruit 库
    #include <bluefruit.h>
    #define SEEED_BLE_NRF52
#elif defined(ESP32)
    // ESP32 使用 NimBLE 库（更轻量）
    #include <NimBLEDevice.h>
    #define SEEED_BLE_ESP32
#else
    #error "Unsupported platform. Use XIAO nRF52840 or XIAO ESP32 series."
#endif

#include <vector>

// =============================================================================
// 版本和常量
// =============================================================================

#define SEEED_BLE_VERSION "1.0.0"

// Seeed Manufacturer ID (0x5EED = "SEED")
#define SEEED_MANUFACTURER_ID 0x5EED

// BTHome Service UUID
#define BTHOME_SERVICE_UUID 0xFCD2

// BTHome 设备信息标志
#define BTHOME_DEVICE_INFO_ENCRYPT  0x01  // 加密
#define BTHOME_DEVICE_INFO_TRIGGER  0x04  // 触发器设备
#define BTHOME_DEVICE_INFO_VERSION  0x40  // BTHome v2

// =============================================================================
// BTHome 传感器类型定义
// =============================================================================

enum BTHomeObjectId : uint8_t {
    // 杂项
    BTHOME_PACKET_ID        = 0x00,  // uint8
    
    // 传感器 - 1 字节
    BTHOME_BATTERY          = 0x01,  // uint8, %
    BTHOME_CO2              = 0x12,  // uint16, ppm
    BTHOME_COUNT_UINT8      = 0x09,  // uint8
    BTHOME_HUMIDITY_UINT8   = 0x2E,  // uint8, %
    BTHOME_MOISTURE_UINT8   = 0x2F,  // uint8, %
    BTHOME_UV_INDEX         = 0x46,  // uint8, UV index
    
    // 传感器 - 2 字节
    BTHOME_TEMPERATURE      = 0x02,  // int16, 0.01°C
    BTHOME_HUMIDITY         = 0x03,  // uint16, 0.01%
    BTHOME_PRESSURE         = 0x04,  // uint24, 0.01 hPa
    BTHOME_ILLUMINANCE      = 0x05,  // uint24, 0.01 lux
    BTHOME_MASS_KG          = 0x06,  // uint16, 0.01 kg
    BTHOME_MASS_LB          = 0x07,  // uint16, 0.01 lb
    BTHOME_DEWPOINT         = 0x08,  // int16, 0.01°C
    BTHOME_COUNT_UINT16     = 0x0A,  // uint16
    BTHOME_COUNT_UINT32     = 0x0B,  // uint32
    BTHOME_ENERGY           = 0x0C,  // uint24, 0.001 kWh
    BTHOME_POWER            = 0x0D,  // uint24, 0.01 W
    BTHOME_VOLTAGE          = 0x0E,  // uint16, 0.001 V
    BTHOME_PM25             = 0x0F,  // uint16, μg/m³
    BTHOME_PM10             = 0x10,  // uint16, μg/m³
    BTHOME_TVOC             = 0x13,  // uint16, μg/m³
    BTHOME_MOISTURE         = 0x14,  // uint16, 0.01%
    BTHOME_DISTANCE_MM      = 0x40,  // uint16, mm
    BTHOME_DISTANCE_M       = 0x41,  // uint16, 0.1m
    BTHOME_DURATION         = 0x42,  // uint24, 0.001s
    BTHOME_CURRENT          = 0x43,  // uint16, 0.001A
    BTHOME_SPEED            = 0x44,  // uint16, 0.01 m/s
    BTHOME_TEMPERATURE_TENTH = 0x45, // int16, 0.1°C
    BTHOME_VOLUME_LITERS    = 0x47,  // uint16, 0.1L
    BTHOME_VOLUME_ML        = 0x48,  // uint16, mL
    BTHOME_VOLUME_FLOW      = 0x49,  // uint16, 0.001 m³/h
    BTHOME_VOLTAGE_TENTH    = 0x4A,  // uint16, 0.1V
    BTHOME_GAS              = 0x4B,  // uint24, 0.001 m³
    BTHOME_GAS_UINT32       = 0x4C,  // uint32, 0.001 m³
    BTHOME_ENERGY_UINT32    = 0x4D,  // uint32, 0.001 kWh
    BTHOME_VOLUME_UINT32    = 0x4E,  // uint32, 0.001 L
    BTHOME_WATER            = 0x4F,  // uint32, 0.001 L
    BTHOME_ROTATION         = 0x3F,  // int16, 0.1°
    
    // 二进制传感器
    BTHOME_BINARY_GENERIC   = 0x0F,  // uint8, 0/1
    BTHOME_BINARY_POWER     = 0x10,  // uint8, 0/1
    BTHOME_BINARY_OPENING   = 0x11,  // uint8, 0/1
    BTHOME_BINARY_BATTERY_LOW = 0x15, // uint8, 0/1
    BTHOME_BINARY_BATTERY_CHARGING = 0x16, // uint8, 0/1
    BTHOME_BINARY_MOTION    = 0x21,  // uint8, 0/1
    BTHOME_BINARY_OCCUPANCY = 0x20,  // uint8, 0/1
    
    // 按钮事件
    BTHOME_BUTTON           = 0x3A,  // uint8, event
};

// 按钮事件类型
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
// 前向声明
// =============================================================================

class SeeedHADiscoveryBLE;
class SeeedBLESensor;

// =============================================================================
// SeeedBLESensor - BLE 传感器类
// =============================================================================

/**
 * BLE 传感器类 - 代表一个通过 BLE 广播的传感器
 */
class SeeedBLESensor {
public:
    /**
     * 构造函数
     * @param objectId BTHome 对象 ID
     */
    SeeedBLESensor(BTHomeObjectId objectId);

    /**
     * 设置传感器值（整数）
     * @param value 传感器值
     */
    void setValue(int32_t value);

    /**
     * 设置传感器值（浮点数，会自动转换）
     * @param value 传感器值
     */
    void setValue(float value);

    /**
     * 设置二进制状态
     * @param state 状态
     */
    void setState(bool state);

    /**
     * 触发按钮事件
     * @param event 事件类型
     */
    void triggerButton(BTHomeButtonEvent event);

    // 获取器
    BTHomeObjectId getObjectId() const { return _objectId; }
    int32_t getRawValue() const { return _rawValue; }
    bool hasValue() const { return _hasValue; }

    // 获取数据大小
    uint8_t getDataSize() const;

    // 写入数据到缓冲区
    void writeToBuffer(uint8_t* buffer, uint8_t& offset) const;

private:
    BTHomeObjectId _objectId;
    int32_t _rawValue;
    bool _hasValue;

    // 根据类型获取乘数
    float _getMultiplier() const;
};

// =============================================================================
// SeeedHADiscoveryBLE - 主类
// =============================================================================

/**
 * Seeed Home Assistant Discovery BLE 主类
 * 
 * 负责：
 * - BLE 初始化
 * - 传感器管理
 * - BTHome 格式广播
 */
class SeeedHADiscoveryBLE {
public:
    /**
     * 构造函数
     */
    SeeedHADiscoveryBLE();

    /**
     * 析构函数
     */
    ~SeeedHADiscoveryBLE();

    // =========================================================================
    // 配置方法
    // =========================================================================

    /**
     * 设置设备名称
     * @param name 设备名称（最多 20 字符）
     */
    void setDeviceName(const char* name);

    /**
     * 启用调试输出
     * @param enable 是否启用
     */
    void enableDebug(bool enable = true);

    /**
     * 设置广播间隔
     * @param intervalMs 间隔（毫秒），默认 5000
     */
    void setAdvertiseInterval(uint32_t intervalMs);

    /**
     * 设置发射功率
     * @param power 功率级别（平台相关）
     */
    void setTxPower(int8_t power);

    // =========================================================================
    // 初始化
    // =========================================================================

    /**
     * 初始化 BLE
     * @param deviceName 设备名称
     * @return 是否成功
     */
    bool begin(const char* deviceName = "Seeed Sensor");

    /**
     * 停止 BLE
     */
    void stop();

    // =========================================================================
    // 传感器管理
    // =========================================================================

    /**
     * 添加传感器
     * @param objectId BTHome 对象 ID
     * @return 传感器对象指针
     */
    SeeedBLESensor* addSensor(BTHomeObjectId objectId);

    /**
     * 添加温度传感器（便捷方法）
     */
    SeeedBLESensor* addTemperature() { return addSensor(BTHOME_TEMPERATURE); }

    /**
     * 添加湿度传感器（便捷方法）
     */
    SeeedBLESensor* addHumidity() { return addSensor(BTHOME_HUMIDITY); }

    /**
     * 添加电池传感器（便捷方法）
     */
    SeeedBLESensor* addBattery() { return addSensor(BTHOME_BATTERY); }

    /**
     * 添加按钮（便捷方法）
     */
    SeeedBLESensor* addButton() { return addSensor(BTHOME_BUTTON); }

    // =========================================================================
    // 广播
    // =========================================================================

    /**
     * 发送 BLE 广播
     * 会自动构建 BTHome 格式的广播数据
     */
    void advertise();

    /**
     * 手动更新广播数据（不发送）
     */
    void updateAdvertiseData();

    // =========================================================================
    // 状态查询
    // =========================================================================

    /**
     * 检查 BLE 是否已初始化
     */
    bool isRunning() const { return _running; }

    /**
     * 获取设备名称
     */
    const char* getDeviceName() const { return _deviceName; }

private:
    // 设备配置
    char _deviceName[21];
    bool _debug;
    bool _running;
    uint32_t _advertiseInterval;
    int8_t _txPower;
    uint8_t _packetId;

    // 传感器列表
    std::vector<SeeedBLESensor*> _sensors;

    // 广播数据缓冲区
    uint8_t _advData[31];
    uint8_t _advDataLen;

    // 平台特定的 BLE 对象
#ifdef SEEED_BLE_ESP32
    NimBLEAdvertising* _pAdvertising;
#endif

    // 内部方法
    void _buildAdvData();
    void _log(const char* message);
    void _logf(const char* format, ...);
};

#endif // SEEED_HA_DISCOVERY_BLE_H

