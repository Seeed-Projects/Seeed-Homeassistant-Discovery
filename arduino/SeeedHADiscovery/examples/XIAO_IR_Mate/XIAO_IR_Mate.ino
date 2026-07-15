/**
 * IR Mate universal remote example
 * IR Mate 万能红外遥控器示例
 */

// AP hotspot name broadcast while the device has no saved WiFi yet
// 设备尚未配网时对外广播的配网热点名称
const char* PROVISIONING_AP_SSID = "Seeed_IR_Mate";

#include <Adafruit_NeoPixel.h>
#include <SeeedHADiscovery.h>

#include "IRMateInfrared.h"

constexpr uint8_t IR_TRANSMIT_PIN = 3;
constexpr uint8_t IR_RECEIVE_PIN = 4;
constexpr uint8_t TOUCH_PIN = 5;
constexpr uint8_t VIBRATION_PIN = 6;
constexpr uint8_t STATUS_LED_PIN = 7;
constexpr uint8_t RESET_PIN = 9;

constexpr uint32_t TOUCH_DEBOUNCE_MS = 50;
// Quiet period after the last toggle before committing a gesture.
// 最后一次跳变之后的静默窗口,期满再判定手势。
constexpr uint32_t TOUCH_SETTLE_MS = 600;
constexpr uint32_t UNPAIRED_BLINK_INTERVAL_MS = 500;
constexpr uint32_t RESULT_LED_DURATION_MS = 400;
constexpr uint32_t VIBRATION_PULSE_MS = 70;
constexpr uint32_t VIBRATION_GAP_MS = 70;

SeeedHADiscovery ha;
IRMateInfrared infrared(ha, IR_TRANSMIT_PIN, IR_RECEIVE_PIN);
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

bool touchRawState = false;
bool touchStableState = false;
uint32_t touchChangedAt = 0;
// Latching toggle: each debounced edge (either direction) is one tap. Count
// taps inside a burst and commit after a quiet settle window.
// 自锁开关:每一次去抖后的跳变(不分方向)算一次点按。窗口内累计点按次数,静默期满再判定。
uint8_t touchTapCount = 0;
uint32_t firstTapAt = 0;
uint32_t lastEdgeAt = 0;
uint32_t lastRawEdgeAt = 0;
uint32_t feedbackEndsAt = 0;
uint32_t vibrationPhaseEndsAt = 0;
uint32_t lastUnpairedBlinkAt = 0;
uint32_t currentColor = UINT32_MAX;
uint8_t vibrationPulsesRemaining = 0;
bool vibrationMotorOn = false;
bool unpairedBlinkOn = false;
bool learningActive = false;
// While set, briefly blank the white light so a second-press prompt is visible
// 置位期间短暂熄灭白灯,让"再按一次"的提示可见
uint32_t learningPromptBlankUntil = 0;

void setStatusColor(uint32_t color) {
    if (currentColor == color) {
        return;
    }
    currentColor = color;
    statusLed.setPixelColor(0, color);
    statusLed.show();
}

void startVibrationPattern(uint8_t pulseCount) {
    vibrationPulsesRemaining = pulseCount;
    vibrationMotorOn = pulseCount > 0;
    digitalWrite(VIBRATION_PIN, vibrationMotorOn ? HIGH : LOW);
    vibrationPhaseEndsAt = vibrationMotorOn ? millis() + VIBRATION_PULSE_MS : 0;
}

/**
 * Advance the vibration pulse sequence without blocking the main loop
 * 在不阻塞主循环的情况下推进震动脉冲序列
 */
void updateVibrationPattern(uint32_t now) {
    if (vibrationPhaseEndsAt == 0 ||
        static_cast<int32_t>(now - vibrationPhaseEndsAt) < 0) {
        return;
    }

    if (vibrationMotorOn) {
        vibrationMotorOn = false;
        digitalWrite(VIBRATION_PIN, LOW);
        vibrationPulsesRemaining--;
        vibrationPhaseEndsAt = vibrationPulsesRemaining > 0
            ? now + VIBRATION_GAP_MS
            : 0;
        return;
    }

    vibrationMotorOn = true;
    digitalWrite(VIBRATION_PIN, HIGH);
    vibrationPhaseEndsAt = now + VIBRATION_PULSE_MS;
}

void startResultFeedback(bool success) {
    feedbackEndsAt = millis() + RESULT_LED_DURATION_MS;
    setStatusColor(
        success ? statusLed.Color(0, 255, 0) : statusLed.Color(255, 0, 0)
    );
    startVibrationPattern(success ? 1 : 2);
}

void resetTouchGestures() {
    touchTapCount = 0;
    firstTapAt = 0;
    lastEdgeAt = 0;
}

bool dispatchTouchGesture(const char* gesture);

uint32_t touchDeltaMs(uint32_t now, uint32_t since) {
    if (since == 0) {
        return 0;
    }
    return now - since;
}

void commitTouchGesture() {
    if (touchTapCount == 0) {
        return;
    }

    const uint32_t now = millis();
    const uint8_t taps = touchTapCount;
    const uint32_t firstTap = firstTapAt;
    const uint32_t lastEdge = lastEdgeAt;
    const uint32_t settleMs = touchDeltaMs(now, lastEdge);
    const uint32_t spanMs = lastEdge > firstTap ? (lastEdge - firstTap) : 0;

    Serial.printf(
        "[%lu ms] Touch commit | taps=%u span=%lu ms settle=%lu ms"
        " (need>=%u) | first_tap@%lu last_edge@%lu\n",
        static_cast<unsigned long>(now),
        static_cast<unsigned int>(taps),
        static_cast<unsigned long>(spanMs),
        static_cast<unsigned long>(settleMs),
        static_cast<unsigned int>(TOUCH_SETTLE_MS),
        static_cast<unsigned long>(firstTap),
        static_cast<unsigned long>(lastEdge)
    );

    resetTouchGestures();

    if (taps >= 4) {
        dispatchTouchGesture("quadruple");
    } else if (taps == 3) {
        dispatchTouchGesture("triple");
    } else if (taps == 2) {
        dispatchTouchGesture("double");
    } else {
        dispatchTouchGesture("single");
    }
}

bool dispatchTouchGesture(const char* gesture) {
    Serial.printf(
        "[%lu ms] Touch gesture: %s\n",
        static_cast<unsigned long>(millis()),
        gesture
    );
    if (infrared.executeTouchGesture(gesture)) {
        return true;
    }
    startResultFeedback(false);
    return false;
}

void handleTouchButton() {
    bool rawState = digitalRead(TOUCH_PIN) == HIGH;
    uint32_t now = millis();

    if (rawState != touchRawState) {
        touchRawState = rawState;
        touchChangedAt = now;
        const uint32_t sinceRaw = touchDeltaMs(now, lastRawEdgeAt);
        Serial.printf(
            "[%lu ms] touch raw -> %s | +%lu ms since last raw edge\n",
            static_cast<unsigned long>(now),
            rawState ? "HIGH" : "LOW",
            static_cast<unsigned long>(sinceRaw)
        );
        lastRawEdgeAt = now;
    }

    if (now - touchChangedAt < TOUCH_DEBOUNCE_MS || rawState == touchStableState) {
        return;
    }

    touchStableState = rawState;

    if (infrared.isLearning()) {
        resetTouchGestures();
        return;
    }

    // Latching toggle: every debounced edge is one tap, regardless of direction.
    // 自锁开关:每一次去抖后的跳变都算一次点按,不区分是变高还是变低。
    const uint32_t sinceLastEdge = touchDeltaMs(now, lastEdgeAt);
    if (touchTapCount < UINT8_MAX) {
        touchTapCount++;
    }
    if (touchTapCount == 1) {
        firstTapAt = now;
    }
    const uint32_t sinceFirstTap =
        touchTapCount == 1 ? 0U : touchDeltaMs(now, firstTapAt);
    Serial.printf(
        "[%lu ms] touch toggle -> %s | +%lu ms since last edge"
        " | +%lu ms since first tap | tap#%u\n",
        static_cast<unsigned long>(now),
        rawState ? "ON" : "OFF",
        static_cast<unsigned long>(sinceLastEdge),
        static_cast<unsigned long>(sinceFirstTap),
        static_cast<unsigned int>(touchTapCount)
    );
    lastEdgeAt = now;
}

void handleTouchAction() {
    if (touchTapCount == 0 || infrared.isLearning() || lastEdgeAt == 0) {
        return;
    }
    if (static_cast<int32_t>(millis() - lastEdgeAt) <
        static_cast<int32_t>(TOUCH_SETTLE_MS)) {
        return;
    }
    commitTouchGesture();
}

void updateStatus() {
    uint32_t now = millis();
    updateVibrationPattern(now);

    if (feedbackEndsAt != 0) {
        if (static_cast<int32_t>(now - feedbackEndsAt) < 0) {
            return;
        }
        feedbackEndsAt = 0;
    }

    if (learningActive) {
        bool blanking = learningPromptBlankUntil != 0 &&
                        static_cast<int32_t>(now - learningPromptBlankUntil) < 0;
        if (blanking) {
            setStatusColor(0);
        } else {
            learningPromptBlankUntil = 0;
            setStatusColor(statusLed.Color(255, 255, 255));
        }
        return;
    }

    if (ha.isHAConnected()) {
        setStatusColor(0);
        return;
    }

    if (now - lastUnpairedBlinkAt >= UNPAIRED_BLINK_INTERVAL_MS) {
        lastUnpairedBlinkAt = now;
        unpairedBlinkOn = !unpairedBlinkOn;
        setStatusColor(unpairedBlinkOn ? statusLed.Color(0, 0, 255) : 0);
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(TOUCH_PIN, INPUT_PULLDOWN);
    pinMode(VIBRATION_PIN, OUTPUT);
    digitalWrite(VIBRATION_PIN, LOW);

    statusLed.begin();
    statusLed.setBrightness(180);
    setStatusColor(statusLed.Color(0, 0, 255));

    ha.setDeviceInfo("IR Mate", "Seeed IR Mate", "1.0.0");
    ha.enableDebug(true);

    infrared.setDefaultCarrierFrequency(38000);
    infrared.onSignalReceived([](size_t timingCount) {
        Serial.printf(
            "IR signal received: %u timings\n",
            static_cast<unsigned int>(timingCount)
        );
    });
    infrared.onTransmitCompleted([](bool success) {
        Serial.printf("IR transmission: %s\n", success ? "success" : "failed");
        startResultFeedback(success);
    });
    infrared.onLearningStateChanged([](bool active) {
        learningActive = active;
        if (active) {
            resetTouchGestures();
        } else {
            learningPromptBlankUntil = 0;
        }
        Serial.printf("IR learning: %s\n", active ? "started" : "stopped");
    });
    infrared.onLearningPrompt([](uint8_t pass) {
        // White light means "press the remote key now"; the second pass adds a
        // short blank + buzz so the user knows to repeat the same key.
        // 白灯代表"现在按遥控键";第二遍加一次短熄灭+震动,提示用户重复按同一个键。
        Serial.printf("IR learning: press the remote key now (%u/2)\n", pass);
        if (pass >= 2) {
            startVibrationPattern(1);
            learningPromptBlankUntil = millis() + 180;
        }
    });
    infrared.onLearningCompleted([](bool success) {
        Serial.printf(
            "IR learning finished: %s\n",
            success ? "verified and saved" : "not saved (see reason above)"
        );
        startResultFeedback(success);
    });
    infrared.begin();
    Serial.println("Offline Gree control ready");
    Serial.println("Touch gestures: latching toggle, edge-count v4 (single/double/triple/quadruple)");

    // Long press the reset button to clear WiFi and restart provisioning
    // 长按重置按钮可清除 WiFi 并重新进入配网模式
    ha.enableResetButton(RESET_PIN, true);

    // Build a per-device hotspot name from the chip MAC suffix
    // 用芯片 MAC 后缀拼出每台设备唯一的配网热点名称
    char apSsid[40];
    snprintf(
        apSsid,
        sizeof(apSsid),
        "%s_%04X",
        PROVISIONING_AP_SSID,
        static_cast<uint16_t>(ESP.getEfuseMac() & 0xFFFF)
    );

    // Connect using saved credentials, or open the provisioning hotspot
    // 使用已保存的凭据连接，否则开启配网热点
    if (ha.beginWithProvisioning(apSsid)) {
        Serial.printf("WiFi connected: %s\n", ha.getLocalIP().toString().c_str());
    } else {
        Serial.printf(
            "Provisioning hotspot started: %s (open http://192.168.4.1)\n",
            apSsid
        );
    }
}

void loop() {
    ha.handle();
    infrared.handle();
    handleTouchButton();
    handleTouchAction();
    updateStatus();
    delay(2);
}
