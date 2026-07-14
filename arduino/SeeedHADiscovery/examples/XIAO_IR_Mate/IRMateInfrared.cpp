#include "IRMateInfrared.h"

#include <limits.h>
#include <string.h>

namespace {
constexpr char STORAGE_NAMESPACE[] = "ir_mate";
constexpr char GREE_STATE_KEY[] = "gree_state";
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
    _learningStateCallback(nullptr)
{
}

void IRMateInfrared::begin() {
    if (_started) {
        return;
    }

    _sender.begin();
    _gree.begin();
    _loadDefaultGreeState();

    _ha.onProtocolMessage("ir_transmit", [this](JsonDocument& doc) {
        _handleTransmitMessage(doc);
    });
    _ha.onProtocolMessage("ir_learn_start", [this](JsonDocument& doc) {
        _handleLearnStartMessage(doc);
    });
    _ha.onProtocolMessage("ir_learn_cancel", [this](JsonDocument& doc) {
        _handleLearnCancelMessage(doc);
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

    _finishLearning(true, "", timings, timingCount);

    if (_receiveCallback) {
        _receiveCallback(timingCount);
    }

    delete[] timings;
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
    _receiver.enableIRIn();

    if (_learningStateCallback) {
        _learningStateCallback(true);
    }
}

void IRMateInfrared::_handleLearnCancelMessage(JsonDocument& doc) {
    uint32_t requestId = doc["request_id"] | 0;
    if (!_learning || (requestId != 0 && requestId != _learningRequestId)) {
        return;
    }
    _finishLearning(false, "learning_cancelled");
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
        if (_transmitCallback) {
            _transmitCallback(false);
        }
        return false;
    }

    for (uint8_t index = 0; index <= repeatCount; index++) {
        _sender.sendRaw(
            timings.data(),
            static_cast<uint16_t>(timings.size()),
            carrierFrequency
        );
    }

    if (_transmitCallback) {
        _transmitCallback(true);
    }
    return true;
}

void IRMateInfrared::_sendTransmitResult(
    uint32_t requestId,
    bool success,
    const String& error
) {
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

    if (_learningStateCallback) {
        _learningStateCallback(false);
    }

    _sendLearningResult(requestId, success, error, timings, timingCount);
}

void IRMateInfrared::_sendLearningResult(
    uint32_t requestId,
    bool success,
    const String& error,
    const uint16_t* timings,
    uint16_t timingCount
) {
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
