/**
 * ============================================================================
 * Seeed Home Assistant Discovery BLE - 实现文件
 * Seeed Home Assistant Discovery BLE - Implementation
 * ============================================================================
 *
 * 支持平台 | Supported Platforms:
 * - ESP32 系列 (NimBLE)
 * - XIAO nRF52840 mbed 版本 (ArduinoBLE)
 * - XIAO nRF52840 Adafruit BSP (Bluefruit)
 *
 * @author limengdu
 */

#include "SeeedHADiscoveryBLE.h"
#include <stdarg.h>

// =============================================================================
// 全局回调指针（用于平台回调）
// =============================================================================

static SeeedHADiscoveryBLE* g_pInstance = nullptr;

// =============================================================================
// ESP32 NimBLE 回调类
// =============================================================================

#ifdef SEEED_BLE_ESP32

class SeeedBLEServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        if (g_pInstance) {
            g_pInstance->_onConnect();
        }
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        if (g_pInstance) {
            g_pInstance->_onDisconnect();
        }
        // 重新开始广播
        NimBLEDevice::startAdvertising();
    }
};

class SeeedBLECharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        if (g_pInstance) {
            NimBLEAttValue value = pChar->getValue();
            g_pInstance->_handleCommand(value.data(), value.length());
        }
    }
};

static SeeedBLEServerCallbacks serverCallbacks;
static SeeedBLECharCallbacks charCallbacks;

#endif

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
        case BTHOME_PRESSURE:
        case BTHOME_ILLUMINANCE:
        case BTHOME_POWER:
            return 100.0f;
        case BTHOME_VOLTAGE:
        case BTHOME_CURRENT:
        case BTHOME_ENERGY:
        case BTHOME_GAS:
        case BTHOME_VOLUME_FLOW:
        case BTHOME_WATER:
        case BTHOME_VOLUME_UINT32:
        case BTHOME_GAS_UINT32:
        case BTHOME_ENERGY_UINT32:
        case BTHOME_DURATION:
            return 1000.0f;
        case BTHOME_MASS_KG:
        case BTHOME_MASS_LB:
            return 100.0f;
        case BTHOME_TEMPERATURE_TENTH:
        case BTHOME_ROTATION:
        case BTHOME_DISTANCE_M:
        case BTHOME_VOLUME_LITERS:
        case BTHOME_VOLTAGE_TENTH:
            return 10.0f;
        default:
            return 1.0f;
    }
}

uint8_t SeeedBLESensor::getDataSize() const {
    switch (_objectId) {
        case BTHOME_BATTERY:
        case BTHOME_COUNT_UINT8:
        case BTHOME_HUMIDITY_UINT8:
        case BTHOME_MOISTURE_UINT8:
        case BTHOME_UV_INDEX:
        case BTHOME_BINARY_GENERIC:
        case BTHOME_BINARY_POWER:
        case BTHOME_BINARY_OPENING:
        case BTHOME_BINARY_BATTERY_LOW:
        case BTHOME_BINARY_BATTERY_CHARGING:
        case BTHOME_BINARY_MOTION:
        case BTHOME_BINARY_OCCUPANCY:
        case BTHOME_BUTTON:
            return 1;
        case BTHOME_PRESSURE:
        case BTHOME_ILLUMINANCE:
        case BTHOME_ENERGY:
        case BTHOME_POWER:
        case BTHOME_DURATION:
        case BTHOME_GAS:
            return 3;
        case BTHOME_COUNT_UINT32:
        case BTHOME_GAS_UINT32:
        case BTHOME_ENERGY_UINT32:
        case BTHOME_VOLUME_UINT32:
        case BTHOME_WATER:
            return 4;
        default:
            return 2;
    }
}

void SeeedBLESensor::writeToBuffer(uint8_t* buffer, uint8_t& offset) const {
    if (!_hasValue) return;
    buffer[offset++] = _objectId;
    uint8_t dataSize = getDataSize();
    for (uint8_t i = 0; i < dataSize; i++) {
        buffer[offset++] = (_rawValue >> (i * 8)) & 0xFF;
    }
}

// =============================================================================
// SeeedBLESwitch 实现
// =============================================================================

SeeedBLESwitch::SeeedBLESwitch(const char* id, const char* name)
    : _state(false)
    , _callback(nullptr)
    , _parent(nullptr)
{
    strncpy(_id, id, sizeof(_id) - 1);
    _id[sizeof(_id) - 1] = '\0';
    strncpy(_name, name, sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';
}

void SeeedBLESwitch::setState(bool state) {
    if (_state != state) {
        _state = state;
        if (_parent) {
            _parent->_notifyStateChange();
        }
    }
}

void SeeedBLESwitch::toggle() {
    setState(!_state);
}

void SeeedBLESwitch::_handleCommand(bool state) {
    _state = state;
    if (_callback) {
        _callback(state);
    }
    if (_parent) {
        _parent->_notifyStateChange();
    }
}

// =============================================================================
// SeeedHADiscoveryBLE 实现
// =============================================================================

SeeedHADiscoveryBLE::SeeedHADiscoveryBLE()
    : _debug(false)
    , _running(false)
    , _connected(false)
    , _controlEnabled(false)
    , _advertiseInterval(5000)
    , _txPower(0)
    , _packetId(0)
    , _advDataLen(0)
#ifdef SEEED_BLE_ESP32
    , _pServer(nullptr)
    , _pControlService(nullptr)
    , _pCommandChar(nullptr)
    , _pStateChar(nullptr)
    , _pAdvertising(nullptr)
#elif defined(SEEED_BLE_MBED_NRF52840)
    , _pControlService(nullptr)
    , _pCommandChar(nullptr)
    , _pStateChar(nullptr)
#endif
{
    strcpy(_deviceName, "Seeed Sensor");
    memset(_advData, 0, sizeof(_advData));
    g_pInstance = this;
}

SeeedHADiscoveryBLE::~SeeedHADiscoveryBLE() {
    stop();
    for (auto sensor : _sensors) delete sensor;
    for (auto sw : _switches) delete sw;
    _sensors.clear();
    _switches.clear();
    g_pInstance = nullptr;
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
    return begin(deviceName, false);
}

bool SeeedHADiscoveryBLE::begin(const char* deviceName, bool enableControl) {
    if (deviceName) {
        setDeviceName(deviceName);
    }
    _controlEnabled = enableControl;

    _log("====================================");
    _log("Seeed HA Discovery BLE v1.1.0");
    _log("====================================");

// =============================================================================
// ESP32 - NimBLE
// =============================================================================
#ifdef SEEED_BLE_ESP32
    _log(SEEED_BLE_PLATFORM);

    NimBLEDevice::init(_deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    if (_controlEnabled) {
        // 创建 GATT 服务器
        _pServer = NimBLEDevice::createServer();
        _pServer->setCallbacks(&serverCallbacks);

        // 创建控制服务
        _pControlService = _pServer->createService(SEEED_CONTROL_SERVICE_UUID);

        // 命令特征值（可写）
        _pCommandChar = _pControlService->createCharacteristic(
            SEEED_CONTROL_COMMAND_CHAR_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
        );
        _pCommandChar->setCallbacks(&charCallbacks);

        // 状态特征值（可读 + 通知）
        _pStateChar = _pControlService->createCharacteristic(
            SEEED_CONTROL_STATE_CHAR_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        _pControlService->start();
        _log("GATT Control Service started");
    }

    _pAdvertising = NimBLEDevice::getAdvertising();

    if (_controlEnabled) {
        // 可连接广播
        _pAdvertising->addServiceUUID(SEEED_CONTROL_SERVICE_UUID);
    }

    _running = true;
    _log("BLE OK");
    return true;

// =============================================================================
// nRF52840 mbed - ArduinoBLE
// =============================================================================
#elif defined(SEEED_BLE_MBED_NRF52840)
    _log(SEEED_BLE_PLATFORM);

    if (!BLE.begin()) {
        _log("BLE init failed!");
        return false;
    }

    BLE.setLocalName(_deviceName);
    BLE.setDeviceName(_deviceName);

    if (_controlEnabled) {
        // 创建控制服务
        _pControlService = new BLEService(SEEED_CONTROL_SERVICE_UUID);

        // 命令特征值（可写）
        _pCommandChar = new BLECharacteristic(
            SEEED_CONTROL_COMMAND_CHAR_UUID,
            BLEWrite | BLEWriteWithoutResponse,
            64
        );

        // 状态特征值（可读 + 通知）
        _pStateChar = new BLECharacteristic(
            SEEED_CONTROL_STATE_CHAR_UUID,
            BLERead | BLENotify,
            64
        );

        _pControlService->addCharacteristic(*_pCommandChar);
        _pControlService->addCharacteristic(*_pStateChar);

        BLE.addService(*_pControlService);
        BLE.setAdvertisedService(*_pControlService);

        _log("GATT Control Service started");
    }

    _running = true;
    _log("BLE OK");
    return true;

// =============================================================================
// nRF52840 Adafruit - Bluefruit
// =============================================================================
#elif defined(SEEED_BLE_NRF52_BLUEFRUIT)
    _log(SEEED_BLE_PLATFORM);

    Bluefruit.begin();
    Bluefruit.setTxPower(4);
    Bluefruit.setName(_deviceName);

    Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED);
    Bluefruit.Advertising.clearData();

    _running = true;
    _log("BLE OK");
    return true;

#else
    _log("Unsupported platform");
    return false;
#endif
}

void SeeedHADiscoveryBLE::stop() {
    if (!_running) return;

#ifdef SEEED_BLE_ESP32
    NimBLEDevice::deinit();
#elif defined(SEEED_BLE_MBED_NRF52840)
    BLE.stopAdvertise();
    BLE.end();
    if (_pControlService) delete _pControlService;
    if (_pCommandChar) delete _pCommandChar;
    if (_pStateChar) delete _pStateChar;
#elif defined(SEEED_BLE_NRF52_BLUEFRUIT)
    Bluefruit.Advertising.stop();
#endif

    _running = false;
    _log("BLE stopped");
}

void SeeedHADiscoveryBLE::loop() {
#ifdef SEEED_BLE_MBED_NRF52840
    if (_controlEnabled) {
        BLE.poll();

        // 检查连接状态
        BLEDevice central = BLE.central();
        if (central) {
            if (!_connected) {
                _onConnect();
            }

            // 检查命令特征值
            if (_pCommandChar && _pCommandChar->written()) {
                uint8_t buffer[64];
                int len = _pCommandChar->readValue(buffer, sizeof(buffer));
                if (len > 0) {
                    _handleCommand(buffer, len);
                }
            }
        } else {
            if (_connected) {
                _onDisconnect();
            }
        }
    }
#endif
    // ESP32 的回调是自动触发的，不需要在 loop 中轮询
}

String SeeedHADiscoveryBLE::getAddress() {
    if (!_running) {
        return "00:00:00:00:00:00";
    }

#ifdef SEEED_BLE_ESP32
    NimBLEAddress addr = NimBLEDevice::getAddress();
    return String(addr.toString().c_str());
#elif defined(SEEED_BLE_MBED_NRF52840)
    return BLE.address();
#elif defined(SEEED_BLE_NRF52_BLUEFRUIT)
    uint8_t mac[6];
    Bluefruit.getAddr(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    return String(macStr);
#else
    return "00:00:00:00:00:00";
#endif
}

SeeedBLESensor* SeeedHADiscoveryBLE::addSensor(BTHomeObjectId objectId) {
    SeeedBLESensor* sensor = new SeeedBLESensor(objectId);
    _sensors.push_back(sensor);

    if (_debug) {
        Serial.print("[SeeedBLE] Add sensor: 0x");
        Serial.println(objectId, HEX);
    }
    return sensor;
}

SeeedBLESwitch* SeeedHADiscoveryBLE::addSwitch(const char* id, const char* name) {
    SeeedBLESwitch* sw = new SeeedBLESwitch(id, name);
    sw->_setParent(this);
    _switches.push_back(sw);

    if (_debug) {
        Serial.print("[SeeedBLE] Add switch: ");
        Serial.println(id);
    }
    return sw;
}

void SeeedHADiscoveryBLE::_buildAdvData() {
    _advDataLen = 0;

    uint8_t sensorDataLen = 0;
    for (auto sensor : _sensors) {
        if (sensor->hasValue()) {
            sensorDataLen += 1 + sensor->getDataSize();
        }
    }

    uint8_t serviceDataLen = 2 + 1 + sensorDataLen;

    uint8_t offset = 0;

    // Flags
    _advData[offset++] = 0x02;
    _advData[offset++] = 0x01;
    _advData[offset++] = 0x06;

    // Service Data (BTHome)
    _advData[offset++] = serviceDataLen + 1;
    _advData[offset++] = 0x16;
    _advData[offset++] = 0xD2;
    _advData[offset++] = 0xFC;
    _advData[offset++] = BTHOME_DEVICE_INFO_VERSION;

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
        advData.addData(_advData, _advDataLen);
        _pAdvertising->setAdvertisementData(advData);

        NimBLEAdvertisementData scanResponse;
        scanResponse.setName(_deviceName);
        _pAdvertising->setScanResponseData(scanResponse);

        // 重新添加控制服务 UUID（如果启用了控制）
        if (_controlEnabled) {
            // 注意：addServiceUUID 是累积的，不会被 setAdvertisementData 清除
            // 但为了确保，我们每次都重新添加
            _pAdvertising->clearServiceUUIDs();
            _pAdvertising->addServiceUUID(SEEED_CONTROL_SERVICE_UUID);
        }
    }

#elif defined(SEEED_BLE_MBED_NRF52840)
    BLEAdvertisingData advData;
    advData.setFlags(BLEFlagsBREDRNotSupported | BLEFlagsGeneralDiscoverable);

    uint8_t serviceDataLen = _advData[3] - 1;
    uint8_t btHomeDataLen = serviceDataLen - 2;
    advData.setAdvertisedServiceData(0xFCD2, &_advData[7], btHomeDataLen);
    advData.setLocalName(_deviceName);

    // 如果启用了控制，重新设置控制服务广播
    if (_controlEnabled && _pControlService) {
        BLE.setAdvertisedService(*_pControlService);
    }

    BLE.setAdvertisingData(advData);

#elif defined(SEEED_BLE_NRF52_BLUEFRUIT)
    Bluefruit.Advertising.clearData();
    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_FLAGS, &_advData[2], 1);
    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, &_advData[5], _advDataLen - 5);
    Bluefruit.Advertising.addName();
#endif
}

void SeeedHADiscoveryBLE::advertise() {
    if (!_running) {
        _log("Warning: BLE not initialized");
        return;
    }

    _packetId++;
    updateAdvertiseData();

#ifdef SEEED_BLE_ESP32
    if (_pAdvertising) {
        _pAdvertising->start();
        if (_debug) {
            Serial.print("[SeeedBLE] Advertise ID=");
            Serial.print(_packetId);
            Serial.print(", len=");
            Serial.println(_advDataLen);
        }
    }

#elif defined(SEEED_BLE_MBED_NRF52840)
    if (_controlEnabled) {
        BLE.setConnectable(true);
    } else {
        BLE.setConnectable(false);
    }
    BLE.advertise();

    if (_debug) {
        Serial.print("[SeeedBLE] Advertise ID=");
        Serial.print(_packetId);
        Serial.print(", len=");
        Serial.println(_advDataLen);
    }

#elif defined(SEEED_BLE_NRF52_BLUEFRUIT)
    Bluefruit.Advertising.start(0);
    if (_debug) {
        Serial.print("[SeeedBLE] Advertise ID=");
        Serial.print(_packetId);
        Serial.print(", len=");
        Serial.println(_advDataLen);
    }
#endif
}

void SeeedHADiscoveryBLE::_handleCommand(const uint8_t* data, size_t length) {
    if (length < 2) return;

    // 命令格式: [switch_index][state]
    // 或者: [0xFF][switch_index][state] 表示控制指定开关
    
    if (_debug) {
        Serial.print("[SeeedBLE] Received command: ");
        for (size_t i = 0; i < length; i++) {
            Serial.print(data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }

    uint8_t switchIndex = data[0];
    bool state = data[1] != 0;

    if (switchIndex < _switches.size()) {
        _switches[switchIndex]->_handleCommand(state);
        if (_debug) {
            Serial.print("[SeeedBLE] Switch ");
            Serial.print(switchIndex);
            Serial.print(" -> ");
            Serial.println(state ? "ON" : "OFF");
        }
    }
}

void SeeedHADiscoveryBLE::_notifyStateChange() {
    if (!_controlEnabled) return;

    uint8_t buffer[64];
    size_t length = 0;
    _buildStateData(buffer, &length);

#ifdef SEEED_BLE_ESP32
    if (_pStateChar && _connected) {
        _pStateChar->setValue(buffer, length);
        _pStateChar->notify();
    }
#elif defined(SEEED_BLE_MBED_NRF52840)
    if (_pStateChar && _connected) {
        _pStateChar->writeValue(buffer, length);
    }
#endif

    if (_debug) {
        Serial.println("[SeeedBLE] State notified");
    }
}

void SeeedHADiscoveryBLE::_buildStateData(uint8_t* buffer, size_t* length) {
    // 状态数据格式: [switch_count][sw0_state][sw1_state]...
    size_t offset = 0;
    buffer[offset++] = _switches.size();

    for (auto sw : _switches) {
        buffer[offset++] = sw->getState() ? 1 : 0;
    }

    *length = offset;
}

void SeeedHADiscoveryBLE::_onConnect() {
    _connected = true;
    _log("Client connected");
    _notifyStateChange();
}

void SeeedHADiscoveryBLE::_onDisconnect() {
    _connected = false;
    _log("Client disconnected");
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
