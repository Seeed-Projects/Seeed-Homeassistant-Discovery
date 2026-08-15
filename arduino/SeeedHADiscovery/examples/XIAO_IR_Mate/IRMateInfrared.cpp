#include "IRMateInfrared.h"
#include "IRLearningMatcher.h"

#include <limits.h>
#include <string.h>

namespace {
constexpr char STORAGE_NAMESPACE[] = "ir_mate";
constexpr char GREE_STATE_KEY[] = "gree_state";
constexpr char TOUCH_STORAGE_NAMESPACE[] = "ir_touch";
constexpr char TOUCH_REVISION_KEY[] = "revision";
constexpr char TOUCH_SOURCE_KEYS[][9] = {"s_source", "d_source", "t_source", "l_source"};
constexpr char TOUCH_ACTION_KEYS[][9] = {"s_action", "d_action", "t_action", "l_action"};
constexpr char TOUCH_FREQUENCY_KEYS[][7] = {"s_freq", "d_freq", "t_freq", "l_freq"};
constexpr char TOUCH_TIMING_KEYS[][6] = {"s_raw", "d_raw", "t_raw", "l_raw"};
constexpr char TOUCH_MANAGED_KEYS[][6] = {"s_mgd", "d_mgd", "t_mgd", "l_mgd"};

// Print a full timing array so learned and transmitted waveforms can be
// compared pulse by pulse on the serial console.
// 打印完整时序数组，便于在串口逐点比对学习到的与发射出的波形。
void logTimings(
    const char* label,
    uint16_t carrierFrequency,
    const uint16_t* timings,
    uint16_t count
) {
    Serial.printf(
        "%s: %u pulses @%uHz\n",
        label,
        static_cast<unsigned int>(count),
        static_cast<unsigned int>(carrierFrequency)
    );
    Serial.print("  timings: [");
    for (uint16_t index = 0; index < count; index++) {
        Serial.print(timings[index]);
        if (index + 1 < count) {
            Serial.print(", ");
        }
    }
    Serial.println("]");
}
}

IRMateInfrared::IRMateInfrared(
    SeeedHADiscovery& ha,
    uint16_t transmitPin,
    uint16_t receivePin,
    uint16_t captureBufferSize,
    uint8_t captureTimeoutMs
) :
    _ha(ha),
    _sender(transmitPin),
    _receiver(receivePin, captureBufferSize, captureTimeoutMs, false),
    _gree(transmitPin, YAW1F),
    _defaultCarrierFrequency(38000),
    _started(false),
    _learning(false),
    _learningRequestId(0),
    _learningDeadline(0),
    _receiveCallback(nullptr),
    _transmitCallback(nullptr),
    _learningStateCallback(nullptr),
    _learningResultCallback(nullptr),
    _learningPromptCallback(nullptr),
    _learningPass(0),
    _pass2ReadyAt(0),
    _touchRevision(0)
{
}

void IRMateInfrared::begin() {
    if (_started) {
        return;
    }

    _sender.begin();
    _gree.begin();
    _loadDefaultGreeState();
    _loadTouchBindings();

    _ha.onProtocolMessage("ir_transmit", [this](JsonDocument& doc) {
        _handleTransmitMessage(doc);
    });
    _ha.onProtocolMessage("ir_builtin_command", [this](JsonDocument& doc) {
        _handleBuiltinCommandMessage(doc);
    });
    _ha.onProtocolMessage("ir_learn_start", [this](JsonDocument& doc) {
        _handleLearnStartMessage(doc);
    });
    _ha.onProtocolMessage("ir_learn_cancel", [this](JsonDocument& doc) {
        _handleLearnCancelMessage(doc);
    });
    _ha.onProtocolMessage("ir_touch_binding_set", [this](JsonDocument& doc) {
        _handleTouchBindingMessage(doc);
    });
    _ha.onProtocolMessage("ir_touch_status_request", [this](JsonDocument& doc) {
        _handleTouchStatusMessage(doc);
    });
    _ha.onDiscovery([this](JsonArray& entities) {
        _appendDiscoveryEntities(entities);
    });

    _started = true;
}

void IRMateInfrared::handle() {
    if (!_started || !_learning) {
        return;
    }

    if (static_cast<int32_t>(millis() - _learningDeadline) >= 0) {
        _finishLearning(false, "learning_timeout");
        return;
    }

    if (!_receiver.decode(&_capture)) {
        return;
    }

    if (_capture.overflow) {
        _receiver.resume();
        return;
    }

    // Skip echoes of the first press so the second pass captures a fresh push.
    // 跳过第一次按键的回声,让第二遍采集到全新的一次按压。
    if (_learningPass == 2 &&
        static_cast<int32_t>(millis() - _pass2ReadyAt) < 0) {
        _receiver.resume();
        return;
    }

    uint16_t timingCount = getCorrectedRawLength(&_capture);
    if (timingCount < MIN_LEARN_TIMINGS || timingCount > MAX_TRANSMIT_ENTRIES) {
        _receiver.resume();
        return;
    }

    uint16_t* timings = resultToRawArray(&_capture);
    if (timings == nullptr) {
        _receiver.resume();
        return;
    }

    if (_learningPass <= 1) {
        // First pass: remember this capture, then ask for a second press.
        // 第一遍:记住这段波形,然后请求再按一次。
        logTimings("IR learn pass 1", _defaultCarrierFrequency, timings, timingCount);
        _firstCapture.assign(timings, timings + timingCount);
        delete[] timings;
        _learningPass = 2;
        _pass2ReadyAt = millis() + LEARN_PASS_GAP_MS;
        _receiver.resume();
        if (_learningPromptCallback) {
            _learningPromptCallback(2);
        }
        return;
    }

    // Second pass: classify stable and dynamic-state waveforms before saving.
    // 第二遍:保存前区分稳定波形与动态状态波形。
    std::vector<uint16_t> secondCapture(timings, timings + timingCount);
    logTimings("IR learn pass 2", _defaultCarrierFrequency, timings, timingCount);
    delete[] timings;

    ir_learning::MatchResult match = ir_learning::classify(
        _firstCapture,
        secondCapture
    );
    if (match.kind == ir_learning::MatchKind::None) {
        Serial.println("IR learn mismatch: the two presses did not match");
        _finishLearning(false, "learn_mismatch");
        return;
    }
    if (match.kind == ir_learning::MatchKind::Dynamic) {
        Serial.printf(
            "IR learn dynamic signal: %u/%u timings matched; saving pass 1\n",
            static_cast<unsigned int>(match.matchingTimingCount),
            static_cast<unsigned int>(_firstCapture.size())
        );
    }

    size_t matchedCount = _firstCapture.size();
    _finishLearning(
        true,
        "",
        _firstCapture.data(),
        static_cast<uint16_t>(matchedCount)
    );

    if (_receiveCallback) {
        _receiveCallback(matchedCount);
    }
}

bool IRMateInfrared::toggleDefaultGreePower() {
    if (!_started || _learning) {
        return false;
    }

    if (_gree.getPower()) {
        _gree.off();
    } else {
        _gree.setMode(kGreeCool);
        _gree.on();
    }
    return _sendDefaultGreeState();
}

bool IRMateInfrared::increaseDefaultGreeTemperature() {
    if (!_started || _learning || !_gree.getPower()) {
        return false;
    }

    uint8_t temperature = _gree.getTemp();
    if (temperature >= kGreeMaxTempC) {
        return false;
    }
    _gree.setTemp(temperature + 1);
    return _sendDefaultGreeState();
}

bool IRMateInfrared::decreaseDefaultGreeTemperature() {
    if (!_started || _learning || !_gree.getPower()) {
        return false;
    }

    uint8_t temperature = _gree.getTemp();
    if (temperature <= kGreeMinTempC) {
        return false;
    }
    _gree.setTemp(temperature - 1);
    return _sendDefaultGreeState();
}

bool IRMateInfrared::cycleDefaultGreeMode() {
    if (!_started || _learning || !_gree.getPower()) {
        return false;
    }

    switch (_gree.getMode()) {
        case kGreeCool:
            _gree.setMode(kGreeDry);
            break;
        case kGreeDry:
            _gree.setMode(kGreeFan);
            break;
        case kGreeFan:
            _gree.setMode(kGreeHeat);
            break;
        case kGreeHeat:
            _gree.setMode(kGreeAuto);
            break;
        default:
            _gree.setMode(kGreeCool);
            break;
    }
    return _sendDefaultGreeState();
}

bool IRMateInfrared::executeTouchGesture(const String& gesture) {
    int8_t index = _gestureIndex(gesture);
    if (index < 0 || _learning) {
        return false;
    }

    TouchBinding& binding = _touchBindings[static_cast<size_t>(index)];
    if (binding.source == TouchBindingSource::Builtin) {
        return _executeBuiltinAction(binding.action);
    }
    if (binding.source == TouchBindingSource::Managed) {
        // Online: let Home Assistant compute the stateful frame; offline: emit
        // the stored fallback frame so the gesture still does something useful.
        // 在线:交给 HA 计算有状态帧;离线:发射存好的兜底帧,让手势仍有作用。
        if (_ha.isHAConnected()) {
            _sendGestureUplink(gesture, binding.managedAction);
            return true;
        }
        if (binding.timings.empty()) {
            return false;
        }
        bool success = _transmitRaw(binding.timings, binding.carrierFrequency, 0);
        if (_transmitCallback) {
            _transmitCallback(success);
        }
        return success;
    }
    if (binding.source != TouchBindingSource::Raw || binding.timings.empty()) {
        return false;
    }

    bool success = _transmitRaw(binding.timings, binding.carrierFrequency, 0);
    if (_transmitCallback) {
        _transmitCallback(success);
    }
    return success;
}

void IRMateInfrared::setDefaultCarrierFrequency(uint16_t frequency) {
    if (frequency >= 1000) {
        _defaultCarrierFrequency = frequency;
    }
}

void IRMateInfrared::onSignalReceived(ReceiveCallback callback) {
    _receiveCallback = callback;
}

void IRMateInfrared::onTransmitCompleted(TransmitCallback callback) {
    _transmitCallback = callback;
}

void IRMateInfrared::onLearningStateChanged(LearningStateCallback callback) {
    _learningStateCallback = callback;
}

void IRMateInfrared::onLearningCompleted(LearningResultCallback callback) {
    _learningResultCallback = callback;
}

void IRMateInfrared::onLearningPrompt(LearningPromptCallback callback) {
    _learningPromptCallback = callback;
}

void IRMateInfrared::_appendDiscoveryEntities(JsonArray& entities) {
    JsonObject transmitter = entities.add<JsonObject>();
    transmitter["id"] = "ir_transmitter";
    transmitter["name"] = "Infrared transmitter";
    transmitter["type"] = "infrared";
    transmitter["role"] = "emitter";

    JsonObject receiver = entities.add<JsonObject>();
    receiver["id"] = "ir_receiver";
    receiver["name"] = "Infrared receiver";
    receiver["type"] = "infrared";
    receiver["role"] = "receiver";
}

void IRMateInfrared::_handleTransmitMessage(JsonDocument& doc) {
    uint32_t requestId = doc["request_id"] | 0;
    uint32_t carrierFrequency = doc["carrier_frequency"] | _defaultCarrierFrequency;
    uint32_t repeatCount = doc["repeat_count"] | 0;

    if (carrierFrequency < 1000 || carrierFrequency > UINT16_MAX) {
        _sendTransmitResult(requestId, false, "carrier_frequency_out_of_range");
        return;
    }

    if (repeatCount > MAX_REPEAT_COUNT) {
        _sendTransmitResult(requestId, false, "repeat_count_out_of_range");
        return;
    }

    JsonArray timings = doc["timings"].as<JsonArray>();
    std::vector<uint16_t> rawTimings;
    String error;
    if (!_parseTimings(timings, rawTimings, error)) {
        _sendTransmitResult(requestId, false, error);
        return;
    }

    bool success = _transmitRaw(
        rawTimings,
        static_cast<uint16_t>(carrierFrequency),
        static_cast<uint8_t>(repeatCount)
    );
    _sendTransmitResult(requestId, success, success ? "" : "transmit_failed");
}

void IRMateInfrared::_handleBuiltinCommandMessage(JsonDocument& doc) {
    uint32_t requestId = doc["request_id"] | 0;
    String actionName = doc["action"] | "";
    BuiltinAction action = _parseBuiltinAction(actionName);
    if (requestId == 0 || action == BuiltinAction::None) {
        _sendTransmitResult(
            requestId,
            false,
            "unsupported_builtin_action"
        );
        return;
    }

    bool success = _executeBuiltinAction(action);
    _sendTransmitResult(
        requestId,
        success,
        success ? "" : "builtin_action_failed",
        !success
    );
}

void IRMateInfrared::_handleLearnStartMessage(JsonDocument& doc) {
    uint32_t requestId = doc["request_id"] | 0;
    uint32_t timeoutMs = doc["timeout_ms"] | 30000;

    if (requestId == 0) {
        _sendLearningResult(0, false, "request_id_required", nullptr, 0);
        return;
    }
    if (timeoutMs < MIN_LEARNING_TIMEOUT_MS || timeoutMs > MAX_LEARNING_TIMEOUT_MS) {
        _sendLearningResult(requestId, false, "learning_timeout_out_of_range", nullptr, 0);
        return;
    }

    if (_learning) {
        _finishLearning(false, "learning_replaced");
    }

    _learning = true;
    _learningRequestId = requestId;
    _learningDeadline = millis() + timeoutMs;
    _learningPass = 1;
    _firstCapture.clear();
    _pass2ReadyAt = 0;
    _receiver.enableIRIn();

    if (_learningStateCallback) {
        _learningStateCallback(true);
    }
    if (_learningPromptCallback) {
        _learningPromptCallback(1);
    }
}

void IRMateInfrared::_handleLearnCancelMessage(JsonDocument& doc) {
    uint32_t requestId = doc["request_id"] | 0;
    if (!_learning || (requestId != 0 && requestId != _learningRequestId)) {
        return;
    }
    _finishLearning(false, "learning_cancelled");
}

void IRMateInfrared::_handleTouchBindingMessage(JsonDocument& doc) {
    uint32_t requestId = doc["request_id"] | 0;
    uint32_t revision = doc["revision"] | 0;
    String gesture = doc["gesture"] | "";
    bool finalBinding = doc["final"] | false;
    int8_t index = _gestureIndex(gesture);
    if (requestId == 0 || index < 0) {
        _sendTouchBindingResult(requestId, false, gesture, "invalid_binding_request");
        return;
    }

    JsonObject bindingData = doc["binding"].as<JsonObject>();
    String source = bindingData["source"] | "";
    TouchBinding binding;
    if (source == "none") {
        binding.source = TouchBindingSource::None;
    } else if (source == "builtin") {
        binding.action = _parseBuiltinAction(bindingData["action"] | "");
        if (binding.action == BuiltinAction::None) {
            _sendTouchBindingResult(
                requestId,
                false,
                gesture,
                "unsupported_builtin_action"
            );
            return;
        }
        binding.source = TouchBindingSource::Builtin;
    } else if (source == "raw") {
        uint32_t carrierFrequency = bindingData["carrier_frequency"] | 0;
        if (carrierFrequency < 1000 || carrierFrequency > UINT16_MAX) {
            _sendTouchBindingResult(
                requestId,
                false,
                gesture,
                "carrier_frequency_out_of_range"
            );
            return;
        }
        String error;
        if (!_parseTimings(bindingData["timings"].as<JsonArray>(), binding.timings, error)) {
            _sendTouchBindingResult(requestId, false, gesture, error);
            return;
        }
        binding.source = TouchBindingSource::Raw;
        binding.carrierFrequency = static_cast<uint16_t>(carrierFrequency);
    } else if (source == "managed") {
        String action = bindingData["action"] | "";
        if (action.length() == 0) {
            _sendTouchBindingResult(requestId, false, gesture, "invalid_managed_action");
            return;
        }
        binding.source = TouchBindingSource::Managed;
        binding.managedAction = action;
        // Optional offline fallback frame emitted when Home Assistant is away.
        // 可选的离线兜底帧,当 Home Assistant 不在线时发射。
        JsonObject fallback = bindingData["fallback"].as<JsonObject>();
        if (!fallback.isNull()) {
            uint32_t carrierFrequency = fallback["carrier_frequency"] | 0;
            std::vector<uint16_t> fallbackTimings;
            String error;
            if (carrierFrequency >= 1000 && carrierFrequency <= UINT16_MAX &&
                _parseTimings(fallback["timings"].as<JsonArray>(), fallbackTimings, error)) {
                binding.carrierFrequency = static_cast<uint16_t>(carrierFrequency);
                binding.timings = std::move(fallbackTimings);
            }
        }
    } else {
        _sendTouchBindingResult(requestId, false, gesture, "unsupported_binding_source");
        return;
    }

    size_t bindingIndex = static_cast<size_t>(index);
    TouchBinding previousBinding = _touchBindings[bindingIndex];
    _touchBindings[bindingIndex] = std::move(binding);
    if (!_saveTouchBinding(bindingIndex)) {
        _touchBindings[bindingIndex] = std::move(previousBinding);
        _sendTouchBindingResult(requestId, false, gesture, "storage_write_failed");
        return;
    }
    if (finalBinding) {
        uint32_t previousRevision = _touchRevision;
        _touchRevision = revision;
        if (!_saveTouchRevision()) {
            _touchRevision = previousRevision;
            _sendTouchBindingResult(
                requestId,
                false,
                gesture,
                "storage_write_failed"
            );
            return;
        }
    }
    _sendTouchBindingResult(requestId, true, gesture);
}

void IRMateInfrared::_handleTouchStatusMessage(JsonDocument& doc) {
    _sendTouchStatusResult(doc["request_id"] | 0);
}

bool IRMateInfrared::_parseTimings(
    JsonArray timings,
    std::vector<uint16_t>& output,
    String& error
) {
    if (timings.isNull() || timings.size() == 0) {
        error = "timings_required";
        return false;
    }

    output.clear();
    output.reserve(timings.size());

    for (size_t index = 0; index < timings.size(); index++) {
        int32_t signedDuration = timings[index].as<int32_t>();
        if (signedDuration == INT32_MIN) {
            error = "timing_out_of_range";
            return false;
        }

        bool expectsMark = (index % 2) == 0;
        if (signedDuration != 0 && ((signedDuration > 0) != expectsMark)) {
            error = "invalid_timing_sequence";
            return false;
        }

        uint32_t duration = static_cast<uint32_t>(
            signedDuration < 0 ? -signedDuration : signedDuration
        );
        while (duration > UINT16_MAX) {
            if (output.size() + 2 > MAX_TRANSMIT_ENTRIES) {
                error = "signal_too_long";
                return false;
            }
            output.push_back(UINT16_MAX);
            output.push_back(0);
            duration -= UINT16_MAX;
        }

        if (output.size() >= MAX_TRANSMIT_ENTRIES) {
            error = "signal_too_long";
            return false;
        }
        output.push_back(static_cast<uint16_t>(duration));
    }

    return true;
}

bool IRMateInfrared::_transmitRaw(
    const std::vector<uint16_t>& timings,
    uint16_t carrierFrequency,
    uint8_t repeatCount
) {
    if (!_started || _learning || timings.empty()) {
        return false;
    }

    logTimings(
        "IR TX",
        carrierFrequency,
        timings.data(),
        static_cast<uint16_t>(timings.size())
    );

    for (uint8_t index = 0; index <= repeatCount; index++) {
        _sender.sendRaw(
            timings.data(),
            static_cast<uint16_t>(timings.size()),
            carrierFrequency
        );
    }

    return true;
}

void IRMateInfrared::_sendTransmitResult(
    uint32_t requestId,
    bool success,
    const String& error,
    bool notifyCallback
) {
    if (notifyCallback && _transmitCallback) {
        _transmitCallback(success);
    }

    JsonDocument doc;
    doc["type"] = "ir_transmit_result";
    doc["request_id"] = requestId;
    doc["success"] = success;
    if (!success && error.length() > 0) {
        doc["error"] = error;
    }
    _ha.sendProtocolMessage(doc);
}

void IRMateInfrared::_finishLearning(
    bool success,
    const String& error,
    const uint16_t* timings,
    uint16_t timingCount
) {
    if (!_learning) {
        return;
    }

    uint32_t requestId = _learningRequestId;
    _receiver.disableIRIn();
    _learning = false;
    _learningRequestId = 0;
    _learningDeadline = 0;

    if (!success && error.length() > 0) {
        Serial.println("IR learn stopped: " + error);
    }

    if (_learningStateCallback) {
        _learningStateCallback(false);
    }

    _sendLearningResult(requestId, success, error, timings, timingCount);

    // Clear the two-pass buffers only after the result has been serialized,
    // because the success payload points at the first capture above.
    // 在结果序列化之后再清空两遍缓冲,因为上面的成功负载指向第一段波形。
    _learningPass = 0;
    _pass2ReadyAt = 0;
    _firstCapture.clear();
}

void IRMateInfrared::_sendLearningResult(
    uint32_t requestId,
    bool success,
    const String& error,
    const uint16_t* timings,
    uint16_t timingCount
) {
    if (_learningResultCallback) {
        _learningResultCallback(success);
    }

    JsonDocument doc;
    doc["type"] = "ir_learn_result";
    doc["request_id"] = requestId;
    doc["entity_id"] = "ir_receiver";
    doc["success"] = success;
    if (!success) {
        if (error.length() > 0) {
            doc["error"] = error;
        }
        _ha.sendProtocolMessage(doc);
        return;
    }

    doc["carrier_frequency"] = _defaultCarrierFrequency;
    JsonArray values = doc["timings"].to<JsonArray>();

    for (uint16_t index = 0; index < timingCount; index++) {
        int32_t duration = timings[index];
        values.add((index % 2) == 0 ? duration : -duration);
    }

    _ha.sendProtocolMessage(doc);
}

void IRMateInfrared::_loadDefaultGreeState() {
    uint8_t state[kGreeStateLength];
    Preferences preferences;
    bool loaded = false;

    if (preferences.begin(STORAGE_NAMESPACE, true)) {
        if (preferences.getBytesLength(GREE_STATE_KEY) == kGreeStateLength) {
            preferences.getBytes(GREE_STATE_KEY, state, sizeof(state));
            if (IRGreeAC::validChecksum(state)) {
                _gree.setRaw(state);
                loaded = true;
            }
        }
        preferences.end();
    }

    if (loaded) {
        return;
    }

    _gree.stateReset();
    _gree.setModel(YAW1F);
    _gree.setMode(kGreeCool);
    _gree.setFan(kGreeFanAuto);
    _gree.setTemp(DEFAULT_GREE_TEMPERATURE);
    _gree.off();
    _saveDefaultGreeState();
}

void IRMateInfrared::_saveDefaultGreeState() {
    Preferences preferences;
    if (!preferences.begin(STORAGE_NAMESPACE, false)) {
        return;
    }

    uint8_t state[kGreeStateLength];
    memcpy(state, _gree.getRaw(), sizeof(state));
    preferences.putBytes(GREE_STATE_KEY, state, sizeof(state));
    preferences.end();
}

bool IRMateInfrared::_sendDefaultGreeState() {
    if (!_started || _learning) {
        return false;
    }

    _gree.send();
    _saveDefaultGreeState();

    if (_transmitCallback) {
        _transmitCallback(true);
    }
    return true;
}

int8_t IRMateInfrared::_gestureIndex(const String& gesture) const {
    if (gesture == "single") {
        return 0;
    }
    if (gesture == "double") {
        return 1;
    }
    if (gesture == "triple") {
        return 2;
    }
    if (gesture == "quadruple") {
        return 3;
    }
    return -1;
}

IRMateInfrared::BuiltinAction IRMateInfrared::_parseBuiltinAction(
    const String& action
) const {
    if (action == "gree_power") {
        return BuiltinAction::GreePower;
    }
    if (action == "gree_temp_up") {
        return BuiltinAction::GreeTemperatureUp;
    }
    if (action == "gree_temp_down") {
        return BuiltinAction::GreeTemperatureDown;
    }
    if (action == "gree_mode") {
        return BuiltinAction::GreeMode;
    }
    return BuiltinAction::None;
}

bool IRMateInfrared::_executeBuiltinAction(BuiltinAction action) {
    switch (action) {
        case BuiltinAction::GreePower:
            return toggleDefaultGreePower();
        case BuiltinAction::GreeTemperatureUp:
            return increaseDefaultGreeTemperature();
        case BuiltinAction::GreeTemperatureDown:
            return decreaseDefaultGreeTemperature();
        case BuiltinAction::GreeMode:
            return cycleDefaultGreeMode();
        default:
            return false;
    }
}

void IRMateInfrared::_setFactoryTouchBindings() {
    _touchBindings[0].source = TouchBindingSource::Builtin;
    _touchBindings[0].action = BuiltinAction::GreePower;
    _touchBindings[1].source = TouchBindingSource::Builtin;
    _touchBindings[1].action = BuiltinAction::GreeTemperatureUp;
    _touchBindings[2].source = TouchBindingSource::Builtin;
    _touchBindings[2].action = BuiltinAction::GreeTemperatureDown;
    _touchBindings[3].source = TouchBindingSource::Builtin;
    _touchBindings[3].action = BuiltinAction::GreeMode;
}

void IRMateInfrared::_loadTouchBindings() {
    _setFactoryTouchBindings();
    Preferences preferences;
    if (!preferences.begin(TOUCH_STORAGE_NAMESPACE, true)) {
        return;
    }

    _touchRevision = preferences.getULong(TOUCH_REVISION_KEY, 0);
    for (size_t index = 0; index < TOUCH_BINDING_COUNT; index++) {
        if (!preferences.isKey(TOUCH_SOURCE_KEYS[index])) {
            continue;
        }
        uint8_t sourceValue = preferences.getUChar(
            TOUCH_SOURCE_KEYS[index],
            static_cast<uint8_t>(TouchBindingSource::None)
        );
        if (sourceValue > static_cast<uint8_t>(TouchBindingSource::Managed)) {
            continue;
        }

        TouchBinding binding;
        binding.source = static_cast<TouchBindingSource>(sourceValue);
        binding.action = static_cast<BuiltinAction>(preferences.getUChar(
            TOUCH_ACTION_KEYS[index],
            static_cast<uint8_t>(BuiltinAction::None)
        ));
        if (binding.source == TouchBindingSource::Builtin &&
            (binding.action == BuiltinAction::None ||
             binding.action > BuiltinAction::GreeMode)) {
            continue;
        }
        if (binding.source == TouchBindingSource::Managed) {
            binding.managedAction = preferences.getString(
                TOUCH_MANAGED_KEYS[index],
                ""
            );
            if (binding.managedAction.length() == 0) {
                continue;
            }
        }
        binding.carrierFrequency = preferences.getUShort(
            TOUCH_FREQUENCY_KEYS[index],
            _defaultCarrierFrequency
        );

        if (binding.source == TouchBindingSource::Raw ||
            binding.source == TouchBindingSource::Managed) {
            size_t byteCount = preferences.getBytesLength(TOUCH_TIMING_KEYS[index]);
            bool validTimings = byteCount > 0 &&
                byteCount % sizeof(uint16_t) == 0 &&
                byteCount <= MAX_TRANSMIT_ENTRIES * sizeof(uint16_t);
            if (validTimings) {
                binding.timings.resize(byteCount / sizeof(uint16_t));
                preferences.getBytes(
                    TOUCH_TIMING_KEYS[index],
                    binding.timings.data(),
                    byteCount
                );
            } else if (binding.source == TouchBindingSource::Raw) {
                // A raw binding without a valid frame is unusable; keep default.
                // 无有效波形的 raw 绑定不可用;保留默认值。
                continue;
            }
        }
        _touchBindings[index] = std::move(binding);
    }
    preferences.end();
}

bool IRMateInfrared::_saveTouchBinding(size_t index) {
    if (index >= TOUCH_BINDING_COUNT) {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(TOUCH_STORAGE_NAMESPACE, false)) {
        return false;
    }

    const TouchBinding& binding = _touchBindings[index];
    bool success = preferences.putUChar(
        TOUCH_SOURCE_KEYS[index],
        static_cast<uint8_t>(binding.source)
    ) == sizeof(uint8_t);
    success = preferences.putUChar(
        TOUCH_ACTION_KEYS[index],
        static_cast<uint8_t>(binding.action)
    ) == sizeof(uint8_t) && success;
    success = preferences.putUShort(
        TOUCH_FREQUENCY_KEYS[index],
        binding.carrierFrequency
    ) == sizeof(uint16_t) && success;
    if (binding.source == TouchBindingSource::Managed) {
        success = preferences.putString(
            TOUCH_MANAGED_KEYS[index],
            binding.managedAction
        ) == binding.managedAction.length() && success;
    } else {
        preferences.remove(TOUCH_MANAGED_KEYS[index]);
    }
    bool storesTimings = (binding.source == TouchBindingSource::Raw ||
                          binding.source == TouchBindingSource::Managed) &&
                         !binding.timings.empty();
    if (storesTimings) {
        size_t byteCount = binding.timings.size() * sizeof(uint16_t);
        success = preferences.putBytes(
            TOUCH_TIMING_KEYS[index],
            binding.timings.data(),
            byteCount
        ) == byteCount && success;
    } else {
        preferences.remove(TOUCH_TIMING_KEYS[index]);
    }
    preferences.end();
    return success;
}

bool IRMateInfrared::_saveTouchRevision() {
    Preferences preferences;
    if (!preferences.begin(TOUCH_STORAGE_NAMESPACE, false)) {
        return false;
    }
    bool success = preferences.putULong(
        TOUCH_REVISION_KEY,
        _touchRevision
    ) == sizeof(uint32_t);
    preferences.end();
    return success;
}

void IRMateInfrared::_sendTouchBindingResult(
    uint32_t requestId,
    bool success,
    const String& gesture,
    const String& error
) {
    JsonDocument doc;
    doc["type"] = "ir_touch_binding_result";
    doc["request_id"] = requestId;
    doc["gesture"] = gesture;
    doc["success"] = success;
    doc["revision"] = _touchRevision;
    if (!success && error.length() > 0) {
        doc["error"] = error;
    }
    _ha.sendProtocolMessage(doc);
}

void IRMateInfrared::_sendGestureUplink(const String& gesture, const String& action) {
    JsonDocument doc;
    doc["type"] = "ir_gesture";
    doc["gesture"] = gesture;
    doc["action"] = action;
    _ha.sendProtocolMessage(doc);
}

void IRMateInfrared::_sendTouchStatusResult(uint32_t requestId) {
    JsonDocument doc;
    doc["type"] = "ir_touch_status_result";
    doc["request_id"] = requestId;
    doc["success"] = requestId != 0;
    doc["revision"] = _touchRevision;
    if (requestId == 0) {
        doc["error"] = "request_id_required";
    }
    _ha.sendProtocolMessage(doc);
}
