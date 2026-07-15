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

#include <array>
#include <functional>
#include <utility>
#include <vector>

class IRMateInfrared {
public:
    using ReceiveCallback = std::function<void(size_t timingCount)>;
    using TransmitCallback = std::function<void(bool success)>;
    using LearningStateCallback = std::function<void(bool active)>;
    using LearningResultCallback = std::function<void(bool success)>;
    // Invoked before each capture pass so the UI can prompt the next press.
    // 每一遍采集前触发,让界面提示用户按下一次。
    using LearningPromptCallback = std::function<void(uint8_t pass)>;

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
    bool cycleDefaultGreeMode();
    bool executeTouchGesture(const String& gesture);
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

    /**
     * Register a callback that prompts the user before each capture pass
     * 注册在每一遍采集前提示用户按键的回调函数
     */
    void onLearningPrompt(LearningPromptCallback callback);

private:
    static constexpr uint16_t MAX_TRANSMIT_ENTRIES = 2048;
    // Minimum pulses that still count as a signal; short remotes are allowed.
    // 仍算作有效信号的最少脉冲数;允许短信号的遥控器。
    static constexpr uint16_t MIN_LEARN_TIMINGS = 4;
    // Two-pass verification tolerances and the gap that skips same-press echoes.
    // 两遍校验的容差,以及用于跳过同一次按键回声的间隔。
    static constexpr uint16_t LEARN_MATCH_TOLERANCE_US = 250;
    static constexpr uint8_t LEARN_MATCH_TOLERANCE_PERCENT = 25;
    static constexpr uint32_t LEARN_PASS_GAP_MS = 250;
    static constexpr uint8_t MAX_REPEAT_COUNT = 10;
    static constexpr uint32_t MIN_LEARNING_TIMEOUT_MS = 1000;
    static constexpr uint32_t MAX_LEARNING_TIMEOUT_MS = 60000;
    static constexpr uint8_t DEFAULT_GREE_TEMPERATURE = 25;
    static constexpr size_t TOUCH_BINDING_COUNT = 4;

    enum class TouchBindingSource : uint8_t {
        None = 0,
        Builtin = 1,
        Raw = 2,
        Managed = 3,
    };

    enum class BuiltinAction : uint8_t {
        None = 0,
        GreePower = 1,
        GreeTemperatureUp = 2,
        GreeTemperatureDown = 3,
        GreeMode = 4,
    };

    struct TouchBinding {
        TouchBindingSource source = TouchBindingSource::None;
        BuiltinAction action = BuiltinAction::None;
        uint16_t carrierFrequency = 38000;
        std::vector<uint16_t> timings;
        // For Managed bindings: the Home-Assistant-side action name, plus an
        // optional offline fallback frame stored in carrierFrequency/timings.
        // Managed 绑定专用:HA 侧动作名,以及存于 carrierFrequency/timings 的可选离线兜底帧。
        String managedAction;
    };

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
    LearningPromptCallback _learningPromptCallback;
    // Two-pass learning state: current pass (1 or 2), the first capture to
    // compare against, and the earliest time the second pass may capture.
    // 两遍学习状态:当前是第几遍(1 或 2)、用于比对的第一段波形、以及第二遍最早可采集的时间。
    uint8_t _learningPass;
    std::vector<uint16_t> _firstCapture;
    uint32_t _pass2ReadyAt;
    std::array<TouchBinding, TOUCH_BINDING_COUNT> _touchBindings;
    uint32_t _touchRevision;

    void _appendDiscoveryEntities(JsonArray& entities);
    void _handleTransmitMessage(JsonDocument& doc);
    void _handleBuiltinCommandMessage(JsonDocument& doc);
    void _handleLearnStartMessage(JsonDocument& doc);
    void _handleLearnCancelMessage(JsonDocument& doc);
    void _handleTouchBindingMessage(JsonDocument& doc);
    void _handleTouchStatusMessage(JsonDocument& doc);
    bool _parseTimings(JsonArray timings, std::vector<uint16_t>& output, String& error);
    bool _timingsMatch(
        const std::vector<uint16_t>& first,
        const std::vector<uint16_t>& second
    ) const;
    bool _transmitRaw(
        const std::vector<uint16_t>& timings,
        uint16_t carrierFrequency,
        uint8_t repeatCount
    );
    void _sendTransmitResult(
        uint32_t requestId,
        bool success,
        const String& error = "",
        bool notifyCallback = true
    );
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
    int8_t _gestureIndex(const String& gesture) const;
    BuiltinAction _parseBuiltinAction(const String& action) const;
    bool _executeBuiltinAction(BuiltinAction action);
    void _setFactoryTouchBindings();
    void _loadTouchBindings();
    bool _saveTouchBinding(size_t index);
    bool _saveTouchRevision();
    void _sendTouchBindingResult(
        uint32_t requestId,
        bool success,
        const String& gesture,
        const String& error = ""
    );
    void _sendTouchStatusResult(uint32_t requestId);
    void _sendGestureUplink(const String& gesture, const String& action);
};

#endif
