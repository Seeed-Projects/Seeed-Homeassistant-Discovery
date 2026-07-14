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
#include <SeeedHADiscovery.h>

#include <functional>
#include <vector>

class IRMateInfrared {
public:
    using ReceiveCallback = std::function<void(size_t timingCount)>;
    using TransmitCallback = std::function<void(bool success)>;

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
     * Start the infrared hardware and register Home Assistant protocol handlers
     * 启动红外硬件并注册 Home Assistant 协议处理函数
     */
    void begin();

    /**
     * Process received infrared signals
     * 处理接收到的红外信号
     */
    void handle();

    /**
     * Replay the latest received signal
     * 重放最近一次接收的信号
     */
    bool replayLastSignal(uint8_t repeatCount = 0);

    bool hasLastSignal() const { return !_lastSignal.empty(); }
    size_t getLastSignalLength() const { return _lastSignal.size(); }

    void setDefaultCarrierFrequency(uint16_t frequency);
    uint16_t getDefaultCarrierFrequency() const { return _defaultCarrierFrequency; }

    void onSignalReceived(ReceiveCallback callback);
    void onTransmitCompleted(TransmitCallback callback);

private:
    static constexpr uint16_t MAX_TRANSMIT_ENTRIES = 2048;
    static constexpr uint8_t MAX_REPEAT_COUNT = 10;

    SeeedHADiscovery& _ha;
    IRsend _sender;
    IRrecv _receiver;
    decode_results _capture;
    std::vector<uint16_t> _lastSignal;
    uint16_t _defaultCarrierFrequency;
    bool _started;
    ReceiveCallback _receiveCallback;
    TransmitCallback _transmitCallback;

    void _appendDiscoveryEntities(JsonArray& entities);
    void _handleTransmitMessage(JsonDocument& doc);
    bool _parseTimings(JsonArray timings, std::vector<uint16_t>& output, String& error);
    bool _transmitRaw(
        const std::vector<uint16_t>& timings,
        uint16_t carrierFrequency,
        uint8_t repeatCount
    );
    void _sendTransmitResult(uint32_t requestId, bool success, const String& error = "");
    void _sendReceivedSignal(const uint16_t* timings, uint16_t timingCount);
};

#endif
