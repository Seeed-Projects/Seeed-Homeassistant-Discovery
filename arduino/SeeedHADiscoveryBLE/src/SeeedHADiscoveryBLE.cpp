/**
 * ============================================================================
 * Seeed Home Assistant Discovery BLE - 实现文件
 * Seeed Home Assistant Discovery BLE - Implementation
 * ============================================================================
 *
 * @author limengdu
 */

#include "SeeedHADiscoveryBLE.h"
#include <stdarg.h>

// =============================================================================
// SeeedBLESensor 实现
// =============================================================================

SeeedBLESensor::SeeedBLESensor(BTHomeObjectId objectId)
    : _objectId(objectId)
    , _rawValue(0)
    , _hasValue(false)
{
}

void SeeedBLESensor::setValue(int32_t value) {
    _rawValue = value;
    _hasValue = true;
}

void SeeedBLESensor::setValue(float value) {
    // 根据对象类型转换浮点数到整数
    float multiplier = _getMultiplier();
    _rawValue = (int32_t)(value * multiplier);
    _hasValue = true;
}

void SeeedBLESensor::setState(bool state) {
    _rawValue = state ? 1 : 0;
    _hasValue = true;
}

void SeeedBLESensor::triggerButton(BTHomeButtonEvent event) {
    _rawValue = event;
    _hasValue = true;
}

float SeeedBLESensor::_getMultiplier() const {
    switch (_objectId) {
        case BTHOME_TEMPERATURE:
        case BTHOME_HUMIDITY:
        case BTHOME_DEWPOINT:
        case BTHOME_MOISTURE:
        case BTHOME_SPEED:
            return 100.0f;  // 0.01 精度

        case BTHOME_PRESSURE:
        case BTHOME_ILLUMINANCE:
        case BTHOME_POWER:
            return 100.0f;  // 0.01 精度

        case BTHOME_VOLTAGE:
        case BTHOME_CURRENT:
        case BTHOME_ENERGY:
        case BTHOME_GAS:
        case BTHOME_VOLUME_FLOW:
        case BTHOME_WATER:
        case BTHOME_VOLUME_UINT32:
        case BTHOME_GAS_UINT32:
        case BTHOME_ENERGY_UINT32:
            return 1000.0f;  // 0.001 精度

        case BTHOME_MASS_KG:
        case BTHOME_MASS_LB:
            return 100.0f;  // 0.01 精度

        case BTHOME_TEMPERATURE_TENTH:
        case BTHOME_ROTATION:
        case BTHOME_DISTANCE_M:
        case BTHOME_VOLUME_LITERS:
        case BTHOME_VOLTAGE_TENTH:
            return 10.0f;  // 0.1 精度

        default:
            return 1.0f;  // 整数
    }
}

uint8_t SeeedBLESensor::getDataSize() const {
    switch (_objectId) {
        // 1 字节数据
        case BTHOME_BATTERY:
        case BTHOME_COUNT_UINT8:
        case BTHOME_HUMIDITY_UINT8:
        case BTHOME_MOISTURE_UINT8:
        case BTHOME_UV_INDEX:
        case BTHOME_BINARY_GENERIC:
        case BTHOME_BINARY_MOTION:
        case BTHOME_BINARY_OCCUPANCY:
        case BTHOME_BUTTON:
            return 1;  // Object ID + 1 byte

        // 2 字节数据
        case BTHOME_TEMPERATURE:
        case BTHOME_HUMIDITY:
        case BTHOME_DEWPOINT:
        case BTHOME_MASS_KG:
        case BTHOME_MASS_LB:
        case BTHOME_COUNT_UINT16:
        case BTHOME_VOLTAGE:
        case BTHOME_PM25:
        case BTHOME_PM10:
        case BTHOME_CO2:
        case BTHOME_TVOC:
        case BTHOME_MOISTURE:
        case BTHOME_DISTANCE_MM:
        case BTHOME_DISTANCE_M:
        case BTHOME_CURRENT:
        case BTHOME_SPEED:
        case BTHOME_TEMPERATURE_TENTH:
        case BTHOME_VOLUME_LITERS:
        case BTHOME_VOLUME_ML:
        case BTHOME_VOLUME_FLOW:
        case BTHOME_VOLTAGE_TENTH:
        case BTHOME_ROTATION:
            return 2;  // Object ID + 2 bytes

        // 3 字节数据
        case BTHOME_PRESSURE:
        case BTHOME_ILLUMINANCE:
        case BTHOME_ENERGY:
        case BTHOME_POWER:
        case BTHOME_DURATION:
        case BTHOME_GAS:
            return 3;  // Object ID + 3 bytes

        // 4 字节数据
        case BTHOME_COUNT_UINT32:
        case BTHOME_GAS_UINT32:
        case BTHOME_ENERGY_UINT32:
        case BTHOME_VOLUME_UINT32:
        case BTHOME_WATER:
            return 4;  // Object ID + 4 bytes

        default:
            return 2;  // 默认 2 字节
    }
}

void SeeedBLESensor::writeToBuffer(uint8_t* buffer, uint8_t& offset) const {
    if (!_hasValue) return;

    // 写入 Object ID
    buffer[offset++] = _objectId;

    // 写入数据（小端序）
    uint8_t dataSize = getDataSize();
    for (uint8_t i = 0; i < dataSize; i++) {
        buffer[offset++] = (_rawValue >> (i * 8)) & 0xFF;
    }
}

// =============================================================================
// SeeedHADiscoveryBLE 实现
// =============================================================================

SeeedHADiscoveryBLE::SeeedHADiscoveryBLE()
    : _debug(false)
    , _running(false)
    , _advertiseInterval(5000)
    , _txPower(0)
    , _packetId(0)
    , _advDataLen(0)
#ifdef SEEED_BLE_ESP32
    , _pAdvertising(nullptr)
#endif
{
    strcpy(_deviceName, "Seeed Sensor");
    memset(_advData, 0, sizeof(_advData));
}

SeeedHADiscoveryBLE::~SeeedHADiscoveryBLE() {
    stop();
    for (auto sensor : _sensors) {
        delete sensor;
    }
    _sensors.clear();
}

void SeeedHADiscoveryBLE::setDeviceName(const char* name) {
    strncpy(_deviceName, name, 20);
    _deviceName[20] = '\0';
}

void SeeedHADiscoveryBLE::enableDebug(bool enable) {
    _debug = enable;
}

void SeeedHADiscoveryBLE::setAdvertiseInterval(uint32_t intervalMs) {
    _advertiseInterval = intervalMs;
}

void SeeedHADiscoveryBLE::setTxPower(int8_t power) {
    _txPower = power;
}

bool SeeedHADiscoveryBLE::begin(const char* deviceName) {
    if (deviceName) {
        setDeviceName(deviceName);
    }

    _log("====================================");
    _log("Seeed HA Discovery BLE 启动中...");
    _log("====================================");

#ifdef SEEED_BLE_ESP32
    // ESP32 使用 NimBLE
    _logf("平台: ESP32 (NimBLE)");
    _logf("设备名称: %s", _deviceName);

    NimBLEDevice::init(_deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // 最大功率

    _pAdvertising = NimBLEDevice::getAdvertising();

    _running = true;
    _log("BLE 初始化完成");
    return true;

#elif defined(SEEED_BLE_NRF52)
    // nRF52840 使用 Bluefruit
    _logf("平台: nRF52840 (Bluefruit)");
    _logf("设备名称: %s", _deviceName);

    Bluefruit.begin();
    Bluefruit.setTxPower(4);  // 4 dBm
    Bluefruit.setName(_deviceName);

    // 设置广播参数
    Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED);
    Bluefruit.Advertising.clearData();

    _running = true;
    _log("BLE 初始化完成");
    return true;

#else
    _log("错误: 不支持的平台");
    return false;
#endif
}

void SeeedHADiscoveryBLE::stop() {
    if (!_running) return;

#ifdef SEEED_BLE_ESP32
    NimBLEDevice::deinit();
#elif defined(SEEED_BLE_NRF52)
    Bluefruit.Advertising.stop();
#endif

    _running = false;
    _log("BLE 已停止");
}

SeeedBLESensor* SeeedHADiscoveryBLE::addSensor(BTHomeObjectId objectId) {
    SeeedBLESensor* sensor = new SeeedBLESensor(objectId);
    _sensors.push_back(sensor);

    _logf("添加传感器: Object ID = 0x%02X", objectId);
    return sensor;
}

void SeeedHADiscoveryBLE::_buildAdvData() {
    _advDataLen = 0;

    // BTHome Service Data 格式:
    // [长度][类型=0x16][UUID低][UUID高][设备信息][传感器数据...]

    // 首先计算传感器数据长度
    uint8_t sensorDataLen = 0;
    for (auto sensor : _sensors) {
        if (sensor->hasValue()) {
            sensorDataLen += 1 + sensor->getDataSize();  // Object ID + Data
        }
    }

    // Service Data 总长度 = UUID(2) + 设备信息(1) + 传感器数据
    uint8_t serviceDataLen = 2 + 1 + sensorDataLen;

    // 构建广播数据
    uint8_t offset = 0;

    // 1. Flags
    _advData[offset++] = 0x02;  // 长度
    _advData[offset++] = 0x01;  // AD Type: Flags
    _advData[offset++] = 0x06;  // LE General Discoverable + BR/EDR Not Supported

    // 2. Service Data (BTHome)
    _advData[offset++] = serviceDataLen + 1;  // 长度（包括类型字节）
    _advData[offset++] = 0x16;  // AD Type: Service Data - 16 bit UUID

    // BTHome Service UUID (0xFCD2, 小端序)
    _advData[offset++] = 0xD2;
    _advData[offset++] = 0xFC;

    // 设备信息字节
    // Bit 0: 加密 (0 = 无加密)
    // Bit 2: 触发器 (0 = 非触发器)
    // Bit 5-7: 版本 (0x02 = BTHome v2)
    _advData[offset++] = BTHOME_DEVICE_INFO_VERSION;  // 0x40 = BTHome v2

    // 传感器数据
    for (auto sensor : _sensors) {
        if (sensor->hasValue()) {
            sensor->writeToBuffer(_advData, offset);
        }
    }

    _advDataLen = offset;
}

void SeeedHADiscoveryBLE::updateAdvertiseData() {
    _buildAdvData();

#ifdef SEEED_BLE_ESP32
    if (_pAdvertising) {
        _pAdvertising->stop();

        NimBLEAdvertisementData advData;
        advData.addData(std::string((char*)_advData, _advDataLen));

        _pAdvertising->setAdvertisementData(advData);

        // 设置设备名称在 Scan Response 中
        NimBLEAdvertisementData scanResponse;
        scanResponse.setName(_deviceName);
        _pAdvertising->setScanResponseData(scanResponse);
    }

#elif defined(SEEED_BLE_NRF52)
    Bluefruit.Advertising.clearData();
    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_FLAGS, &_advData[2], 1);
    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, &_advData[5], _advDataLen - 5);
    Bluefruit.Advertising.addName();
#endif
}

void SeeedHADiscoveryBLE::advertise() {
    if (!_running) {
        _log("警告: BLE 未初始化");
        return;
    }

    // 更新包 ID（用于去重）
    _packetId++;

    // 构建广播数据
    updateAdvertiseData();

#ifdef SEEED_BLE_ESP32
    if (_pAdvertising) {
        _pAdvertising->start();

        if (_debug) {
            _logf("发送 BLE 广播 (ID=%d, 数据长度=%d)", _packetId, _advDataLen);
        }
    }

#elif defined(SEEED_BLE_NRF52)
    Bluefruit.Advertising.start(0);  // 0 = 无超时

    if (_debug) {
        _logf("发送 BLE 广播 (ID=%d, 数据长度=%d)", _packetId, _advDataLen);
    }
#endif
}

void SeeedHADiscoveryBLE::_log(const char* message) {
    if (_debug) {
        Serial.print("[SeeedBLE] ");
        Serial.println(message);
    }
}

void SeeedHADiscoveryBLE::_logf(const char* format, ...) {
    if (_debug) {
        char buffer[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Serial.print("[SeeedBLE] ");
        Serial.println(buffer);
    }
}

