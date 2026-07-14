/**
 * IR Mate infrared module
 * IR Mate 红外收发模块
 */

#ifndef IR_MATE_INFRARED_H
#define IR_MATE_INFRARED_H

#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Preferences.h>
#include <SeeedHADiscovery.h>
#include <ir_Gree.h>

#include <functional>
#include <vector>

class IRMateInfrared {
public:
    using ReceiveCallback = std::function<void(size_t timingCount)>;
    using TransmitCallback = std::function<void(bool success)>;
    using LearningStateCallback = std::function<void(bool active)>;
    using LearningResultCallback = std::function<void(bool success)>;

    /**
     * Create an infrared transmitter and receiver pair
     * 创建一组红外发射器和接收器
     */
    IRMateInfrared(
        SeeedHADiscovery& ha,
        uint16_t transmitPin,
        uint16_t receivePin,
        uint16_t captureBufferSize = 1024,
        uint8_t captureTimeoutMs = 50
    );

    /**
     * Start transmitters and register Home Assistant protocol handlers
     * 启动发射器并注册 Home Assistant 协议处理函数
     */
    void begin();

    /**
     * Process an active Home Assistant learning session
     * 处理由 Home Assistant 启动的学习会话
     */
    void handle();

    /**
     * Send the local Gree air-conditioner controls
     * 发送本地格力空调控制指令
     */
    bool toggleDefaultGreePower();
    bool increaseDefaultGreeTemperature();
    bool decreaseDefaultGreeTemperature();
    bool isLearning() const { return _learning; }

    void setDefaultCarrierFrequency(uint16_t frequency);
    uint16_t getDefaultCarrierFrequency() const { return _defaultCarrierFrequency; }

    void onSignalReceived(ReceiveCallback callback);
    void onTransmitCompleted(TransmitCallback callback);
    void onLearningStateChanged(LearningStateCallback callback);

    /**
     * Register a callback for the final learning result
     * 注册红外学习最终结果的回调函数
     */
    void onLearningCompleted(LearningResultCallback callback);

private:
    static constexpr uint16_t MAX_TRANSMIT_ENTRIES = 2048;
    static constexpr uint16_t MIN_LEARN_TIMINGS = 12;
    static constexpr uint8_t MAX_REPEAT_COUNT = 10;
    static constexpr uint32_t MIN_LEARNING_TIMEOUT_MS = 1000;
    static constexpr uint32_t MAX_LEARNING_TIMEOUT_MS = 60000;
    static constexpr uint8_t DEFAULT_GREE_TEMPERATURE = 25;

    SeeedHADiscovery& _ha;
    IRsend _sender;
    IRrecv _receiver;
    IRGreeAC _gree;
    decode_results _capture;
    uint16_t _defaultCarrierFrequency;
    bool _started;
    bool _learning;
    uint32_t _learningRequestId;
    uint32_t _learningDeadline;
    ReceiveCallback _receiveCallback;
    TransmitCallback _transmitCallback;
    LearningStateCallback _learningStateCallback;
    LearningResultCallback _learningResultCallback;

    void _appendDiscoveryEntities(JsonArray& entities);
    void _handleTransmitMessage(JsonDocument& doc);
    void _handleLearnStartMessage(JsonDocument& doc);
    void _handleLearnCancelMessage(JsonDocument& doc);
    bool _parseTimings(JsonArray timings, std::vector<uint16_t>& output, String& error);
    bool _transmitRaw(
        const std::vector<uint16_t>& timings,
        uint16_t carrierFrequency,
        uint8_t repeatCount
    );
    void _sendTransmitResult(uint32_t requestId, bool success, const String& error = "");
    void _finishLearning(
        bool success,
        const String& error = "",
        const uint16_t* timings = nullptr,
        uint16_t timingCount = 0
    );
    void _sendLearningResult(
        uint32_t requestId,
        bool success,
        const String& error,
        const uint16_t* timings,
        uint16_t timingCount
    );
    void _loadDefaultGreeState();
    void _saveDefaultGreeState();
    bool _sendDefaultGreeState();
};

#endif
