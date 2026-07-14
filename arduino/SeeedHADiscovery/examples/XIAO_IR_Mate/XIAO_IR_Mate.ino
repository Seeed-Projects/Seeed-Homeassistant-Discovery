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

constexpr uint32_t TOUCH_DEBOUNCE_MS = 35;
constexpr uint32_t TOUCH_MULTI_CLICK_MS = 350;
constexpr uint32_t TOUCH_LONG_PRESS_MS = 800;
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
uint32_t lastTouchAt = 0;
uint32_t touchPressedAt = 0;
uint8_t touchCount = 0;
uint32_t feedbackEndsAt = 0;
uint32_t vibrationPhaseEndsAt = 0;
uint32_t lastUnpairedBlinkAt = 0;
uint32_t currentColor = UINT32_MAX;
uint8_t vibrationPulsesRemaining = 0;
bool vibrationMotorOn = false;
bool unpairedBlinkOn = false;
bool longPressHandled = false;

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

void handleTouchButton() {
    bool rawState = digitalRead(TOUCH_PIN) == HIGH;
    uint32_t now = millis();

    if (rawState != touchRawState) {
        touchRawState = rawState;
        touchChangedAt = now;
    }

    if (now - touchChangedAt < TOUCH_DEBOUNCE_MS || rawState == touchStableState) {
        return;
    }

    touchStableState = rawState;
    if (infrared.isLearning()) {
        touchCount = 0;
        longPressHandled = false;
        return;
    }

    if (touchStableState) {
        touchPressedAt = now;
        longPressHandled = false;
        return;
    }

    if (longPressHandled) {
        return;
    }
    if (touchCount < UINT8_MAX) {
        touchCount++;
    }
    lastTouchAt = now;
}

void handleTouchAction() {
    if (touchCount == 0 || millis() - lastTouchAt < TOUCH_MULTI_CLICK_MS) {
        return;
    }

    uint8_t completedCount = touchCount;
    touchCount = 0;

    bool sent = false;
    if (completedCount == 1) {
        Serial.println("Touch gesture: single");
        sent = infrared.executeTouchGesture("single");
    } else if (completedCount == 2) {
        Serial.println("Touch gesture: double");
        sent = infrared.executeTouchGesture("double");
    } else if (completedCount == 3) {
        Serial.println("Touch gesture: triple");
        sent = infrared.executeTouchGesture("triple");
    } else {
        Serial.printf("Touch action ignored: %u touches\n", completedCount);
    }

    if (!sent && completedCount <= 3) {
        startResultFeedback(false);
    }
}

void handleLongPressAction() {
    if (!touchStableState || longPressHandled || infrared.isLearning()) {
        return;
    }
    if (millis() - touchPressedAt < TOUCH_LONG_PRESS_MS) {
        return;
    }

    longPressHandled = true;
    touchCount = 0;
    Serial.println("Touch gesture: long");
    if (!infrared.executeTouchGesture("long")) {
        startResultFeedback(false);
    }
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
        if (active) {
            touchCount = 0;
        }
        Serial.printf("IR learning: %s\n", active ? "started" : "stopped");
    });
    infrared.onLearningCompleted([](bool success) {
        Serial.printf("IR learning result: %s\n", success ? "success" : "failed");
        startResultFeedback(success);
    });
    infrared.begin();
    Serial.println("Offline Gree control ready");

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
    handleLongPressAction();
    handleTouchAction();
    updateStatus();
    delay(2);
}
