#include "IRMateInfrared.h"

#include <limits.h>

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
    _defaultCarrierFrequency(38000),
    _started(false),
    _receiveCallback(nullptr),
    _transmitCallback(nullptr)
{
}

void IRMateInfrared::begin() {
    if (_started) {
        return;
    }

    _sender.begin();
    _receiver.enableIRIn();

    _ha.onProtocolMessage("ir_transmit", [this](JsonDocument& doc) {
        _handleTransmitMessage(doc);
    });
    _ha.onDiscovery([this](JsonArray& entities) {
        _appendDiscoveryEntities(entities);
    });

    _started = true;
}

void IRMateInfrared::handle() {
    if (!_started || !_receiver.decode(&_capture)) {
        return;
    }

    if (_capture.overflow) {
        _receiver.resume();
        return;
    }

    uint16_t timingCount = getCorrectedRawLength(&_capture);
    if (timingCount == 0 || timingCount > MAX_TRANSMIT_ENTRIES) {
        _receiver.resume();
        return;
    }

    uint16_t* timings = resultToRawArray(&_capture);
    if (timings == nullptr) {
        _receiver.resume();
        return;
    }

    _lastSignal.assign(timings, timings + timingCount);
    _sendReceivedSignal(timings, timingCount);

    if (_receiveCallback) {
        _receiveCallback(timingCount);
    }

    delete[] timings;
    _receiver.resume();
}

bool IRMateInfrared::replayLastSignal(uint8_t repeatCount) {
    if (_lastSignal.empty()) {
        return false;
    }

    return _transmitRaw(_lastSignal, _defaultCarrierFrequency, repeatCount);
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
    if (!_started || timings.empty()) {
        if (_transmitCallback) {
            _transmitCallback(false);
        }
        return false;
    }

    _receiver.disableIRIn();
    for (uint8_t index = 0; index <= repeatCount; index++) {
        _sender.sendRaw(
            timings.data(),
            static_cast<uint16_t>(timings.size()),
            carrierFrequency
        );
    }
    _receiver.enableIRIn();

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

void IRMateInfrared::_sendReceivedSignal(
    const uint16_t* timings,
    uint16_t timingCount
) {
    JsonDocument doc;
    doc["type"] = "ir_received";
    doc["entity_id"] = "ir_receiver";
    JsonArray values = doc["timings"].to<JsonArray>();

    for (uint16_t index = 0; index < timingCount; index++) {
        int32_t duration = timings[index];
        values.add((index % 2) == 0 ? duration : -duration);
    }

    _ha.sendProtocolMessage(doc);
}
